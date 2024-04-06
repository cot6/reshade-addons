/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <windows.h>
#include <mmsystem.h>

#include <imgui.h>
#include <reshade.hpp>
#include <forkawesome.h>

#include "dllmain.hpp"
#include "imgui_widgets.hpp"
#include "localization.hpp"
#include "runtime_config.hpp"

#include <fpng.h>
#include <utf8/unchecked.h>

#include <filesystem>
#include <list>
#include <string>
#include <thread>

using namespace reshade::api;

HMODULE g_module_handle;

static const std::wstring_view invalid_characters = L"[]\"/|?*";
static int path_filter(ImGuiInputTextCallbackData *data)
{
    return 0x0 <= data->EventChar && data->EventChar <= 0x1F || data->EventChar == 0x7F || invalid_characters.find(data->EventChar) != std::string::npos;
}

void screenshot_context::save()
{
    config.save(ini_file::load_cache(environment.reshade_base_path / L"ReShade_Addon_Screenshot.ini"), false);
}
inline bool screenshot_context::is_screenshot_active() const noexcept
{
    if (active_screenshot == nullptr)
        return false;
    if (active_screenshot->repeat_count != 0 && active_screenshot->repeat_count <= screenshot_repeat_index)
        return false;

    return true;
}
inline bool screenshot_context::is_screenshot_enable(screenshot_kind kind) const noexcept
{
    return active_screenshot != nullptr && active_screenshot->is_enable(kind);
}
inline bool screenshot_context::is_screenshot_frame() const noexcept
{
    if (active_screenshot == nullptr)
        return false;
    if (active_screenshot->repeat_count != 0 && active_screenshot->repeat_count <= screenshot_repeat_index)
        return false;
    if (active_screenshot->repeat_interval != 0 && (current_frame - screenshot_begin_frame) % active_screenshot->repeat_interval)
        return false;

    return true;
}
inline bool screenshot_context::is_screenshot_frame(screenshot_kind kind) const noexcept
{
    return is_screenshot_frame() && active_screenshot->is_enable(kind);
}

static void on_init(reshade::api::effect_runtime *runtime)
{
    ini_file::flush_cache();

    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->create_private_data<screenshot_context>();

    ctx.active_screenshot = nullptr;

    ctx.environment.load(runtime);
    ctx.config.load(ini_file::load_cache(ctx.environment.addon_screenshot_config_path));

    ctx.screenshot_begin_frame = std::numeric_limits<decltype(ctx.screenshot_begin_frame)>::max();
}
static void on_destroy(reshade::api::device *device)
{
    ini_file::flush_cache();

    device->destroy_private_data<screenshot_context>();
}

static void on_device_present(reshade::api::command_queue *, reshade::api::swapchain *swapchain, const reshade::api::rect *, const reshade::api::rect *, uint32_t, const reshade::api::rect *)
{
    ini_file::flush_cache();

    uint64_t effect_runtime; swapchain->get_private_data(s_runtime_id, &effect_runtime);
    if (effect_runtime == 0)
        return;
    reshade::api::effect_runtime *runtime = reinterpret_cast<reshade::api::effect_runtime *>(effect_runtime);
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();
    if (std::addressof(ctx) == nullptr)
        return;

    ctx.current_frame++;
    ctx.present_time = std::chrono::system_clock::now();

    if (ctx.is_screenshot_frame())
    {
        ctx.capture_last = ctx.capture_time;
        ctx.capture_time = ctx.present_time;
    }

    if (ctx.is_screenshot_frame(screenshot_kind::original))
    {
        screenshot &screenshot = ctx.screenshots.emplace_front(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::original, ctx.screenshot_state, ctx.present_time);
        screenshot.repeat_index = ctx.screenshot_repeat_index;
    }

    if (ctx.screenshotdepth_technique.handle != 0)
    {
        const bool enabled = ctx.is_screenshot_frame(screenshot_kind::depth);

        if (runtime->get_technique_state(ctx.screenshotdepth_technique) != enabled)
            runtime->set_technique_state(ctx.screenshotdepth_technique, enabled);
    }

    if (ctx.config.turn_on_effects == decltype(screenshot_config::turn_on_effects)::turn_on_while_myset_is_active &&
        ctx.is_screenshot_frame(screenshot_kind::after) &&
        !runtime->get_effects_state())
        runtime->set_effects_state(true);
}
static void on_begin_effects(reshade::api::effect_runtime *runtime, reshade::api::command_list *, reshade::api::resource_view, reshade::api::resource_view)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();
    if (std::addressof(ctx) == nullptr)
        return;

    if (ctx.is_screenshot_frame(screenshot_kind::before))
    {
        screenshot &screenshot = ctx.screenshots.emplace_front(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::before, ctx.screenshot_state, ctx.present_time);
        screenshot.repeat_index = ctx.screenshot_repeat_index;
    }
}
static void on_finish_effects(reshade::api::effect_runtime *runtime, reshade::api::command_list *, reshade::api::resource_view, reshade::api::resource_view)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();
    if (std::addressof(ctx) == nullptr)
        return;

    if (ctx.is_screenshot_frame(screenshot_kind::after))
    {
        screenshot &screenshot = ctx.screenshots.emplace_front(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::after, ctx.screenshot_state, ctx.present_time);
        screenshot.repeat_index = ctx.screenshot_repeat_index;
    }

    if (ctx.is_screenshot_frame(screenshot_kind::depth) &&
        ctx.screenshotdepth_technique.handle != 0)
    {
        auto get_texture_data = [runtime, device](reshade::api::resource resource, reshade::api::resource_usage state, std::vector<uint8_t> &texture_data, reshade::api::format &texture_format) -> bool
            {
                const reshade::api::resource_desc desc = device->get_resource_desc(resource);
                texture_format = reshade::api::format_to_default_typed(desc.texture.format, 0);

                if (texture_format != reshade::api::format::r32_float)
                {
                    reshade::log_message(reshade::log_level::error, std::format("Screenshots are not supported for format %u !", desc.texture.format).c_str());
                    return false;
                }

                // Copy back buffer data into system memory buffer
                reshade::api::resource intermediate;
                if (!device->create_resource(reshade::api::resource_desc(desc.texture.width, desc.texture.height, 1, 1, texture_format, 1, reshade::api::memory_heap::gpu_to_cpu, reshade::api::resource_usage::copy_dest), nullptr, reshade::api::resource_usage::copy_dest, &intermediate))
                {
                    reshade::log_message(reshade::log_level::error, "Failed to create system memory texture for screenshot capture!");
                    return false;
                }

                device->set_resource_name(intermediate, "ReShade screenshot texture");

                reshade::api::command_list *const cmd_list = runtime->get_command_queue()->get_immediate_command_list();
                cmd_list->barrier(resource, state, reshade::api::resource_usage::copy_source);
                cmd_list->copy_texture_region(resource, 0, nullptr, intermediate, 0, nullptr);
                cmd_list->barrier(resource, reshade::api::resource_usage::copy_source, state);

                // Wait for any rendering by the application finish before submitting
                // It may have submitted that to a different queue, so simply wait for all to idle here
                runtime->get_command_queue()->wait_idle();

                // Copy data from intermediate image into output buffer
                reshade::api::subresource_data mapped_data = {};
                if (device->map_texture_region(intermediate, 0, nullptr, reshade::api::map_access::read_only, &mapped_data))
                {
                    const uint8_t *mapped_pixels = static_cast<const uint8_t *>(mapped_data.data);
                    const uint32_t pixels_row_pitch = 4u * desc.texture.width;
                    texture_data.resize(4ull * desc.texture.width * desc.texture.height);
                    uint8_t *pixels = texture_data.data();

                    for (uint32_t y = 0; y < desc.texture.height; ++y, pixels += pixels_row_pitch, mapped_pixels += mapped_data.row_pitch)
                        std::memcpy(pixels, mapped_pixels, pixels_row_pitch);

                    device->unmap_texture_region(intermediate, 0);
                }

                device->destroy_resource(intermediate);

                return mapped_data.data != nullptr;
            };

        if (reshade::api::effect_texture_variable texture = runtime->find_texture_variable("__Addon_ScreenshotDepth_Seri14.addonfx", "__Addon_Texture_ScreenshotDepth_Seri14");
            texture.handle != 0)
        {
            reshade::api::resource_view rsv{}, rsv_srgb{};
            if (runtime->get_texture_binding(texture, &rsv, &rsv_srgb);
                rsv.handle != 0)
            {
                if (reshade::api::resource resource = device->get_resource_from_view(rsv);
                    resource.handle != 0)
                {
                    std::vector<uint8_t> texture_data;
                    reshade::api::format texture_format;
                    if (get_texture_data(resource, reshade::api::resource_usage::shader_resource, texture_data, texture_format))
                    {
                        screenshot &screenshot = ctx.screenshots.emplace_front(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::depth, ctx.screenshot_state, ctx.present_time);
                        screenshot.repeat_index = ctx.screenshot_repeat_index;
                        screenshot.pixels = std::move(texture_data);
                    }
                }
            }
        }
    }
}
static void on_reshade_present(reshade::api::effect_runtime *runtime)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();
    if (std::addressof(ctx) == nullptr)
        return;

    if (ctx.is_screenshot_frame(screenshot_kind::overlay))
    {
        screenshot &screenshot = ctx.screenshots.emplace_front(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::overlay, ctx.screenshot_state, ctx.present_time);
        screenshot.repeat_index = ctx.screenshot_repeat_index;
    }

    if (ctx.screenshots.size() > 0)
    {
        for (size_t remain = std::min(ctx.screenshots.size(), ctx.screenshot_worker_threads - ctx.screenshot_active_threads);
            remain > 0;
            remain--)
        {
            ctx.screenshot_active_threads++;

            std::thread screenshot_thread = std::thread(
               [&ctx, screenshot = std::move(ctx.screenshots.back())]() mutable
               {
                   screenshot.save();
                   ctx.screenshot_active_threads--;
               });

            ctx.screenshots.pop_back();
            screenshot_thread.detach();

            if (ctx.screenshots.empty())
                break;
        }
    }

    if (ctx.is_screenshot_frame())
    {
        if (!ctx.active_screenshot->playsound_path.empty() || ctx.active_screenshot->playsound_force)
        {
            if (ctx.screenshot_repeat_index == 0 ||
                ctx.active_screenshot->playback_mode == decltype(screenshot_myset::playback_mode)::playback_every_time)
            {
                ctx.playsound_flags = SND_ASYNC;
                if (ctx.active_screenshot->playsound_as_system_notification)
                    ctx.playsound_flags |= SND_SYSTEM;
                if (!ctx.active_screenshot->playsound_force)
                    ctx.playsound_flags |= SND_NODEFAULT;
                if (ctx.active_screenshot->playsound_path.has_extension())
                    ctx.playsound_flags |= SND_FILENAME;
                else
                    ctx.playsound_flags |= SND_ALIAS;
                if (ctx.active_screenshot->playback_mode == decltype(screenshot_myset::playback_mode)::playback_while_myset_is_active)
                    ctx.playsound_flags |= SND_LOOP;

                PlaySoundW(ctx.active_screenshot->playsound_path.empty() ? L"SystemDefault" : ctx.active_screenshot->playsound_path.c_str(), nullptr, ctx.playsound_flags);
            }
        }
        ctx.screenshot_repeat_index++;
    }

    if (ctx.active_screenshot != nullptr && ctx.active_screenshot->repeat_count != 0 && ctx.active_screenshot->repeat_count <= ctx.screenshot_repeat_index)
    {
        if ((ctx.playsound_flags & SND_LOOP) == SND_LOOP)
        {
            PlaySoundW(nullptr, nullptr, ctx.playsound_flags);
            ctx.playsound_flags = 0;
        }

        if (ctx.effects_state_activated)
            runtime->set_effects_state(false);

        ctx.active_screenshot = nullptr;
    }

    if (!ctx.ignore_shortcuts)
    {
        static auto is_key_down = [runtime](unsigned int keycode) -> bool
            {
                return !keycode || runtime->is_key_down(keycode);
            };
        for (screenshot_myset &screenshot_myset : ctx.config.screenshot_mysets)
        {
            const unsigned int(&keys)[4] = screenshot_myset.screenshot_key_data;

            if (!keys[0])
                continue;

            if (runtime->is_key_pressed(keys[0]) && is_key_down(keys[1]) && is_key_down(keys[2]) && is_key_down(keys[3]))
            {
                if ((ctx.playsound_flags & SND_LOOP) == SND_LOOP)
                {
                    PlaySoundW(nullptr, nullptr, ctx.playsound_flags);
                    ctx.playsound_flags = 0;
                }

                if (ctx.active_screenshot == &screenshot_myset)
                {
                    ctx.active_screenshot = nullptr;

                    if (ctx.effects_state_activated)
                    {
                        ctx.effects_state_activated = false;
                        runtime->set_effects_state(false);
                    }
                }
                else
                {
                    ctx.active_screenshot = &screenshot_myset;
                    ctx.capture_time = std::numeric_limits<decltype(ctx.capture_time)>::max();
                    ctx.capture_last = std::numeric_limits<decltype(ctx.capture_last)>::max();

                    ctx.screenshot_state.reset();

                    ctx.screenshot_begin_frame = ctx.current_frame + 1;
                    ctx.screenshot_repeat_index = 0;

                    if (screenshot_myset.worker_threads != 0)
                        ctx.screenshot_worker_threads = screenshot_myset.worker_threads;
                    else
                        ctx.screenshot_worker_threads = std::thread::hardware_concurrency();

                    switch (ctx.config.turn_on_effects)
                    {
                        case decltype(screenshot_config::turn_on_effects)::turn_on_when_activate_myset:
                        case decltype(screenshot_config::turn_on_effects)::turn_on_while_myset_is_active:
                            if (ctx.is_screenshot_enable(screenshot_kind::after) && !runtime->get_effects_state())
                            {
                                ctx.effects_state_activated = true;
                                runtime->set_effects_state(true);
                            }
                            break;
                        default:
                            ctx.effects_state_activated = false;
                            break;
                    }
                    break;
                }
            }
        }
    }
}
static void on_reshade_overlay(reshade::api::effect_runtime *runtime)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();
    if (std::addressof(ctx) == nullptr)
        return;

    // Disable keyboard shortcuts while typing into input boxes
    ctx.ignore_shortcuts = ImGui::IsAnyItemActive();
}
static void on_reshade_reloaded_effects(reshade::api::effect_runtime *runtime)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();
    if (std::addressof(ctx) == nullptr)
        return;

    ctx.screenshotdepth_technique = runtime->find_technique("__Addon_ScreenshotDepth_Seri14.addonfx", "__Addon_Technique_ScreenshotDepth_Seri14");
}

static const ImVec4 COLOR_RED = ImColor(240, 100, 100);
static const ImVec4 COLOR_YELLOW = ImColor(204, 204, 0);

static void draw_osd_window(reshade::api::effect_runtime *runtime)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();
    if (std::addressof(ctx) == nullptr)
        return;

    bool hide_osd = false;
    switch (ctx.config.show_osd)
    {
        case decltype(screenshot_config::show_osd)::hidden:
            hide_osd = true;
            break;
        case decltype(screenshot_config::show_osd)::show_osd_always:
            hide_osd = false;
            break;
        case decltype(screenshot_config::show_osd)::show_osd_while_myset_is_active:
            hide_osd = ctx.active_screenshot == nullptr && ctx.screenshot_state.error_occurs == 0 && ctx.screenshots.empty();
            break;
        case decltype(screenshot_config::show_osd)::show_osd_while_myset_is_active_ignore_errors:
            hide_osd = ctx.active_screenshot == nullptr;
            break;
    }

    if (!hide_osd && ctx.active_screenshot)
    {
        ImGui::Text("%s", _("Active set: "));
        ImGui::SameLine(0, 0);
        ImGui::TextUnformatted(ctx.active_screenshot->name.c_str(), ctx.active_screenshot->name.c_str() + ctx.active_screenshot->name.size());
    }

    if (!hide_osd)
    {
        float fraction;
        std::string str;
        if (ctx.active_screenshot != nullptr)
        {
            fraction = (float)((ctx.current_frame - ctx.screenshot_begin_frame) % ctx.active_screenshot->repeat_interval) / ctx.active_screenshot->repeat_interval;
            str = std::format(ctx.active_screenshot->repeat_count != 0 ? _("%u of %u") : _("%u times (Infinite mode)"), ctx.screenshot_repeat_index, ctx.active_screenshot->repeat_count);
        }
        else
        {
            fraction = 1.0f;
            str = _("ready");
        }
        ImGui::ProgressBar(fraction, ImVec2(ImGui::GetContentRegionAvail().x, 0), "");
        ImGui::SameLine(15);
        ImGui::Text("%*s", str.size(), str.c_str());

        if (!ctx.screenshots.empty())
        {
            uint64_t using_bytes = 0;
            std::for_each(ctx.screenshots.cbegin(), ctx.screenshots.cend(),
                [&using_bytes](const screenshot &screenshot) {
                        using_bytes += screenshot.pixels.size();
                });
            str = std::format(_("%u shots in queue (%.3lf MiB)"), ctx.screenshots.size(), static_cast<double>(using_bytes) / (1024 * 1024 * 1));
            ImGui::Text("%*s", str.size(), str.c_str());
        }
    }

    if (!hide_osd)
    {
        if (ctx.active_screenshot != nullptr &&
            ctx.screenshot_state.last_elapsed / std::max<size_t>(1, ctx.screenshot_worker_threads) > (ctx.capture_time - ctx.capture_last).count())
            ImGui::TextColored(COLOR_YELLOW, "%s", _("Processing of screenshots is too slow!"));

        if (ctx.screenshot_state.error_occurs > 0)
            ImGui::TextColored(COLOR_YELLOW, "%s", _("Failed. Check details in the log."));

        if ((ctx.is_screenshot_enable(screenshot_kind::before) || ctx.is_screenshot_enable(screenshot_kind::after)) &&
            (ctx.config.turn_on_effects == decltype(screenshot_config::turn_on_effects)::ignore || ctx.config.turn_on_effects == decltype(screenshot_config::turn_on_effects)::turn_on_when_activate_myset) &&
            !runtime->get_effects_state())
            ImGui::TextColored(COLOR_YELLOW, "%s", _("[WARNING] Skipping \"Before\" and \"After\" captures because effects are disabled."));

        if (ctx.is_screenshot_enable(screenshot_kind::depth) &&
            (ctx.config.turn_on_effects == decltype(screenshot_config::turn_on_effects)::ignore || ctx.config.turn_on_effects == decltype(screenshot_config::turn_on_effects)::turn_on_when_activate_myset) &&
            !runtime->get_effects_state())
            ImGui::TextColored(COLOR_YELLOW, "%s", _("[WARNING] Skipping \"Depth\" capture because effects are disabled."));
    }

    // -------------------------------------
    // Important Errors
    // -------------------------------------

    if (ctx.screenshotdepth_technique.handle == 0)
    {
        ImGui::TextColored(COLOR_RED, "%s", _("[ALERT] \"Depth\" capture cannot be performed."));
        ImGui::TextColored(COLOR_RED, "%s", _("Check the log for additional installation steps."));
    }
}
static void draw_setting_window(reshade::api::effect_runtime *runtime)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();
    if (std::addressof(ctx) == nullptr)
        return;

    bool modified = false;

    std::list<std::pair<std::string, reshade::api::effect_technique>> techniques;
    runtime->enumerate_techniques(nullptr,
        [runtime, &techniques](reshade::api::effect_runtime *, reshade::api::effect_technique &technique) {
            if (!runtime->get_technique_state(technique))
                return;
            char technique_name[128] = "";
            runtime->get_technique_name(technique, technique_name);
            techniques.emplace_back(technique_name, technique);
        });

    if (ImGui::CollapsingHeader(_("Screenshot Add-On [by seri14]"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        std::string show_osd_items = _("Hidden\nAlways\nWhile myset is active\nWhile myset is active (Ignore errors)\n");
        std::replace(show_osd_items.begin(), show_osd_items.end(), '\n', '\0');
        std::string turn_on_effects_items = _("Ignore\nWhile myset is active\nWhen activate myset\n");
        std::replace(turn_on_effects_items.begin(), turn_on_effects_items.end(), '\n', '\0');
        modified |= ImGui::Combo(_("Show OSD"), reinterpret_cast<int *>(&ctx.config.show_osd), show_osd_items.c_str());
        modified |= ImGui::Combo(_("Turn On Effects"), reinterpret_cast<int *>(&ctx.config.turn_on_effects), turn_on_effects_items.c_str());

        char buf[4096] = "";
        std::string playback_mode_items = _("Play sound only when first frame is captured\nPlay a sound each time a frame is captured\nPlay sound continuously while capturing frames\n");
        std::replace(playback_mode_items.begin(), playback_mode_items.end(), '\n', '\0');

        for (screenshot_myset &screenshot_myset : ctx.config.screenshot_mysets)
        {
            const auto validate_image_path = [&ctx, &screenshot_myset](screenshot_kind kind, std::filesystem::path &image, std::string &status) {
                if (status.clear(); image.empty() || image.native().front() == '-')
                    return;

                screenshot dummy(nullptr, ctx.environment, screenshot_myset, kind, ctx.screenshot_state, ctx.present_time);
                std::filesystem::path expanded = std::filesystem::u8path(dummy.expand_macro_string(image.u8string()));
                expanded = ctx.environment.reshade_base_path / expanded;

                if (!expanded.has_filename())
                {
                    status = _("File name is mandatory. You will get an error when saving screenshots.");
                    return;
                }

                std::error_code ec;
                std::filesystem::file_status file = std::filesystem::status(expanded, ec);
                if ((ec.value() != 0x0 && ec.value() != 0x2 && ec.value() != 0x3) ||
                    (file.type() != std::filesystem::file_type::not_found && file.type() != std::filesystem::file_type::regular))
                    status = std::format(_("Check failed with %d. You will get an error when saving screenshots.\n%s"), ec.value(), format_message(ec.value(), 0).c_str());
                };

            ImGui::PushID(screenshot_myset.name.c_str(), screenshot_myset.name.c_str() + screenshot_myset.name.size());

            if (ImGui::CollapsingHeader(screenshot_myset.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                modified |= reshade::imgui::key_input_box(_("Screenshot key"), _("When you enter this key combination, the add-on will begin saving screenshots with the following setting. To abort the capture, re-enter the screenshot key."), screenshot_myset.screenshot_key_data, runtime);

                ImGui::BeginDisabled(ctx.is_screenshot_active());

                if (buf[screenshot_myset.playsound_path.u8string().copy(buf, sizeof(buf) - 1)] = '\0';
                    ImGui::InputTextWithHint(_("Screenshot sound"), _("Enter path to play"), buf, sizeof(buf), ImGuiInputTextFlags_CallbackCharFilter, path_filter))
                {
                    modified = true;
                    screenshot_myset.playsound_path = std::filesystem::u8path(buf);
                }
                ImGui::SetItemTooltip(_("Audio file that is played when taking a screenshot."));
                modified |= ImGui::Combo(_("Playback mode"), reinterpret_cast<int *>(&screenshot_myset.playback_mode), playback_mode_items.c_str());
                modified |= ImGui::Checkbox(_("Play default sound if not exists"), &screenshot_myset.playsound_force);
                modified |= ImGui::Checkbox(_("Play sound as system notification"), &screenshot_myset.playsound_as_system_notification);

                ImGui::EndDisabled();

                screenshot_kind screenshot_path_hovered = screenshot_kind::unset;
                if (!screenshot_myset.original_status.empty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, COLOR_RED);
                    ImGui::TextUnformatted(screenshot_myset.original_status.data(), screenshot_myset.original_status.data() + screenshot_myset.original_status.size());
                    ImGui::PopStyleColor();
                }
                if (buf[screenshot_myset.original_image.u8string().copy(buf, sizeof(buf) - 1)] = '\0';
                    ImGui::InputTextWithHint(_("Original image"), _("Enter path to capture"), buf, sizeof(buf), ImGuiInputTextFlags_CallbackCharFilter, path_filter),
                    ImGui::IsItemDeactivatedAfterEdit())
                {
                    modified = true;
                    screenshot_myset.original_image = std::filesystem::u8path(buf);

                    validate_image_path(screenshot_kind::original, screenshot_myset.original_image, screenshot_myset.original_status);
                }
                if (screenshot_path_hovered == screenshot_kind::unset && ImGui::IsItemHovered())
                    screenshot_path_hovered = screenshot_kind::original;
                if (!screenshot_myset.before_status.empty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, COLOR_RED);
                    ImGui::TextUnformatted(screenshot_myset.before_status.data(), screenshot_myset.before_status.data() + screenshot_myset.before_status.size());
                    ImGui::PopStyleColor();
                }
                if (buf[screenshot_myset.before_image.u8string().copy(buf, sizeof(buf) - 1)] = '\0';
                    ImGui::InputTextWithHint(_("Before image"), _("Enter path to capture"), buf, sizeof(buf), ImGuiInputTextFlags_CallbackCharFilter, path_filter),
                    ImGui::IsItemDeactivatedAfterEdit())
                {
                    modified = true;
                    screenshot_myset.before_image = std::filesystem::u8path(buf);

                    validate_image_path(screenshot_kind::before, screenshot_myset.before_image, screenshot_myset.before_status);
                }
                if (screenshot_path_hovered == screenshot_kind::unset && ImGui::IsItemHovered())
                    screenshot_path_hovered = screenshot_kind::before;
                if (!screenshot_myset.after_status.empty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, COLOR_RED);
                    ImGui::TextUnformatted(screenshot_myset.after_status.data(), screenshot_myset.after_status.data() + screenshot_myset.after_status.size());
                    ImGui::PopStyleColor();
                }
                if (buf[screenshot_myset.after_image.u8string().copy(buf, sizeof(buf) - 1)] = '\0';
                    ImGui::InputTextWithHint(_("After image"), _("Enter path to capture"), buf, sizeof(buf), ImGuiInputTextFlags_CallbackCharFilter, path_filter),
                    ImGui::IsItemDeactivatedAfterEdit())
                {
                    modified = true;
                    screenshot_myset.after_image = std::filesystem::u8path(buf);

                    validate_image_path(screenshot_kind::after, screenshot_myset.after_image, screenshot_myset.after_status);
                }
                if (screenshot_path_hovered == screenshot_kind::unset && ImGui::IsItemHovered())
                    screenshot_path_hovered = screenshot_kind::after;
                if (!screenshot_myset.overlay_status.empty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, COLOR_RED);
                    ImGui::TextUnformatted(screenshot_myset.overlay_status.data(), screenshot_myset.overlay_status.data() + screenshot_myset.overlay_status.size());
                    ImGui::PopStyleColor();
                }
                if (buf[screenshot_myset.overlay_image.u8string().copy(buf, sizeof(buf) - 1)] = '\0';
                    ImGui::InputTextWithHint(_("Overlay image"), _("Enter path to capture"), buf, sizeof(buf), ImGuiInputTextFlags_CallbackCharFilter, path_filter),
                    ImGui::IsItemDeactivatedAfterEdit())
                {
                    modified = true;
                    screenshot_myset.overlay_image = std::filesystem::u8path(buf);

                    validate_image_path(screenshot_kind::overlay, screenshot_myset.overlay_image, screenshot_myset.overlay_status);
                }
                if (screenshot_path_hovered == screenshot_kind::unset && ImGui::IsItemHovered())
                    screenshot_path_hovered = screenshot_kind::overlay;
                if (!screenshot_myset.depth_status.empty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, COLOR_RED);
                    ImGui::TextUnformatted(screenshot_myset.depth_status.data(), screenshot_myset.depth_status.data() + screenshot_myset.depth_status.size());
                    ImGui::PopStyleColor();
                }
                if (buf[screenshot_myset.depth_image.u8string().copy(buf, sizeof(buf) - 1)] = '\0';
                    ImGui::InputTextWithHint(_("Depth image"), _("Enter path to capture"), buf, sizeof(buf), ImGuiInputTextFlags_CallbackCharFilter, path_filter),
                    ImGui::IsItemDeactivatedAfterEdit())
                {
                    modified = true;
                    screenshot_myset.depth_image = std::filesystem::u8path(buf);

                    validate_image_path(screenshot_kind::depth, screenshot_myset.depth_image, screenshot_myset.depth_status);
                }
                if (screenshot_path_hovered == screenshot_kind::unset && ImGui::IsItemHovered())
                    screenshot_path_hovered = screenshot_kind::depth;
                if (screenshot_path_hovered != screenshot_kind::unset)
                {
                    std::string tooltip;
                    switch (screenshot_path_hovered)
                    {
                        case original:
                            tooltip += _("Capture the frame rendered by the game.");
                            break;
                        case before:
                            tooltip += _("Capture the frame rendered by the game only if effects are enabled.");
                            break;
                        case after:
                            tooltip += _("Capture the frame after applied the effect only if the effects are enabled.");
                            break;
                        case overlay:
                            tooltip += _("Capture the frame after rendered the ReShade overlay.");
                            break;
                        case depth:
                            tooltip += _("Capture the depth of the frame that was used in the game only if effects are enabled.");
                            break;
                    }
                    tooltip += "\n\n";
                    tooltip += _("Macros you can add that are resolved during saving:\n"
                        "  <APP>               File name of the current executable file (%s)\n"
                        "  <PRESET>            File name of the current preset file (%s)\n"
                        "  <INDEX[:format]>    Current number of continuous screenshot\n"
                        "                      (default: D1)\n"
                        "  <DATE[:format]>     Timestamp of taken screenshot\n"
                        "                      (default: %%Y-%%m-%%d %%H-%%M-%%S)");
                    ImGui::SetTooltip(tooltip.c_str(),
                        ctx.environment.reshade_executable_path.stem().string().c_str(),
                        ctx.environment.reshade_preset_path.stem().string().c_str());
                }
                ImGui::SetItemTooltip(_("Select the image file format.\nHowever, the depth is always saved in TIFF format regardless of this selection."));
                if (ImGui::SliderInt(_("Repeat count"), reinterpret_cast<int *>(&screenshot_myset.repeat_count), 0, 60, screenshot_myset.repeat_count == 0 ? _("infinity") : _("%d times"), ImGuiSliderFlags_None))
                {
                    if (static_cast<int>(screenshot_myset.repeat_count) < 0)
                        screenshot_myset.repeat_count = 1;

                    modified = true;
                }
                ImGui::SetItemTooltip(_("Specify the number of frames to save after pressing the screenshot shortcut key. To abort capture, re-enter the screenshot key."));
                if (ImGui::SliderInt(_("Repeat interval"), reinterpret_cast<int *>(&screenshot_myset.repeat_interval), 1, 60, screenshot_myset.repeat_interval > 1 ? _("%d frames") : _("every frame"), ImGuiSliderFlags_None))
                {
                    if (static_cast<int>(screenshot_myset.repeat_interval) < 1)
                        screenshot_myset.repeat_interval = 1;

                    modified = true;
                }
                ImGui::SetItemTooltip(_("Specify the interval of frames to be save until the specified number of frames."));
                if (ImGui::SliderInt(_("Worker threads"), reinterpret_cast<int *>(&screenshot_myset.worker_threads), 0, std::thread::hardware_concurrency(), screenshot_myset.worker_threads == 0 ? _("unlimited") : _("%d threads")))
                {
                    if (static_cast<int>(screenshot_myset.worker_threads) < 0)
                        screenshot_myset.worker_threads = 1;

                    modified = true;
                }
                ImGui::SetItemTooltip(_("Specify the number of threads to compress the captured frames to specified image files."));
                uint32_t width = 0, height = 0;
                runtime->get_screenshot_width_and_height(&width, &height);
                int enables = 0, depths = 0;
                if (screenshot_myset.is_enable(screenshot_kind::original)) { enables += 1; depths += 4; }
                if (screenshot_myset.is_enable(screenshot_kind::before)) { enables += 1; depths += 4; }
                if (screenshot_myset.is_enable(screenshot_kind::after)) { enables += 1; depths += 4; }
                if (screenshot_myset.is_enable(screenshot_kind::overlay)) { enables += 1; depths += 4; }
                if (screenshot_myset.is_enable(screenshot_kind::depth)) { enables += 1; depths += 4; }
                ImGui::Text(_("Estimate memory usage: %.3lf MiB per once (%d images)"), static_cast<double>(depths * width * height) / (1024 * 1024 * 1), enables);

                int file_write_buffer_size = screenshot_myset.file_write_buffer_size / (1024 * 1);
                if (ImGui::SliderInt("File write buffer size", &file_write_buffer_size, 4, 1024 * 1, "%d KiB"))
                {
                    modified = true;
                    screenshot_myset.file_write_buffer_size = std::clamp(file_write_buffer_size * (1024 * 1), 1024 * 4, std::numeric_limits<int>::max());
                }
                modified |= ImGui::Combo(_("File format"), reinterpret_cast<int *>(&screenshot_myset.image_format),
                    "[libpng] 24-bit PNG\0"
                    "[libpng] 32-bit PNG\0"
                    "[fpng] 24-bit PNG\0"
                    "[fpng] 32-bit PNG\0"
                    "[libtiff] 24-bit TIFF\0"
                    "[libtiff] 32-bit TIFF\0"
                );
                if (screenshot_myset.image_format == 0 || screenshot_myset.image_format == 1)
                {
                    if (ImGui::TreeNodeEx(_("libpng settings###AdvancedSettingsLibpng"), ImGuiTreeNodeFlags_NoTreePushOnOpen))
                    {
                        ImGui::TextUnformatted(_("Presets:")); ImGui::SameLine();
                        if (ImGui::SmallButton(_("High speed###PresetsHighSpeed")))
                        {
                            modified = true;
                            screenshot_myset.zlib_compression_level = Z_BEST_SPEED;
                            screenshot_myset.zlib_compression_strategy = Z_HUFFMAN_ONLY;
                            screenshot_myset.libpng_png_filters = PNG_FAST_FILTERS;
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton(_("High compression###PresetsHighCompression")))
                        {
                            modified = true;
                            screenshot_myset.zlib_compression_level = Z_BEST_COMPRESSION;
                            screenshot_myset.zlib_compression_strategy = Z_RLE;
                            screenshot_myset.libpng_png_filters = PNG_NO_FILTERS;
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton(_("Default###PresetsDefault")))
                        {
                            modified = true;
                            screenshot_myset.zlib_compression_level = Z_BEST_COMPRESSION;
                            screenshot_myset.zlib_compression_strategy = Z_RLE;
                            screenshot_myset.libpng_png_filters = PNG_ALL_FILTERS;
                        }
                        // PNG filter algorithms
                        {
                            ImGui::BeginGroup();

                            const float item_width = ImGui::CalcItemWidth();

                            // Group all radio buttons together into a list
                            ImGui::BeginGroup();

                            bool v = screenshot_myset.libpng_png_filters == PNG_NO_FILTERS;
                            if (ImGui::Checkbox(_("NO FILTERS   (0x00)"), &v))
                            {
                                modified = true;
                                screenshot_myset.libpng_png_filters = PNG_NO_FILTERS;
                            }
                            modified |= ImGui::CheckboxFlags(_("FILTER NONE  (0x08)"), &screenshot_myset.libpng_png_filters, PNG_FILTER_NONE);
                            modified |= ImGui::CheckboxFlags(_("FILTER SUB   (0x10)"), &screenshot_myset.libpng_png_filters, PNG_FILTER_SUB);
                            modified |= ImGui::CheckboxFlags(_("FILTER UP    (0x20)"), &screenshot_myset.libpng_png_filters, PNG_FILTER_UP);
                            modified |= ImGui::CheckboxFlags(_("FILTER AVG   (0x40)"), &screenshot_myset.libpng_png_filters, PNG_FILTER_AVG);
                            modified |= ImGui::CheckboxFlags(_("FILTER PAETH (0x80)"), &screenshot_myset.libpng_png_filters, PNG_FILTER_PAETH);

                            ImGui::EndGroup();

                            ImGui::SameLine(item_width, ImGui::GetStyle().ItemInnerSpacing.x);

                            ImGui::BeginGroup();
                            ImGui::TextUnformatted(_("[libpng] PNG filter algorithms"));
                            ImGui::TextUnformatted(_("Presets:"));
                            if (ImGui::SmallButton(_("FAST###PNGFastFilters")))
                            {
                                modified = true;
                                screenshot_myset.libpng_png_filters = PNG_FAST_FILTERS;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton(_("ALL###PNGAllFilters")))
                            {
                                modified = true;
                                screenshot_myset.libpng_png_filters = PNG_ALL_FILTERS;
                            }
                            ImGui::EndGroup();

                            ImGui::EndGroup();
                        }
                        modified |= ImGui::SliderInt(_("[zlib] Compression level"), &screenshot_myset.zlib_compression_level, Z_DEFAULT_COMPRESSION, Z_BEST_COMPRESSION,
                            screenshot_myset.zlib_compression_level == Z_DEFAULT_COMPRESSION ? _("Default compression") :
                            screenshot_myset.zlib_compression_level == Z_NO_COMPRESSION ? _("No compression") :
                            screenshot_myset.zlib_compression_level == Z_BEST_SPEED ? _("Best speed") :
                            screenshot_myset.zlib_compression_level == Z_BEST_COMPRESSION ? _("Best compression") : "%d", ImGuiSliderFlags_AlwaysClamp);
                        std::string compression_strategy_items = _("DEFAULT STRATEGY\nFILTERED\nHUFFMAN ONLY\nRLE\nFIXED\n");
                        for (char *c = compression_strategy_items.data(); *c != '\0'; c++)
                        {
                            if (*c == '\n')
                                *c = '\0';
                        }
                        modified |= reshade::imgui::radio_list(_("[zlib] Compression strategy"), compression_strategy_items, screenshot_myset.zlib_compression_strategy);
                    }
                }
            }

            ImGui::PopID();
        }
        ImGui::TextUnformatted(_("Add set:"));
        ImGui::SameLine();
        if (ImGui::Button(ICON_FK_PLUS) || ctx.config.screenshot_mysets.empty())
        {
            const ini_file &config_file = ini_file::load_cache(ctx.environment.addon_screenshot_config_path);
            ctx.config.screenshot_mysets.emplace_back(config_file, "myset" + std::to_string(ctx.present_time.time_since_epoch().count()));
            modified = true;
        }
    }

    if (modified)
        ctx.save();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        g_module_handle = hModule;

        fpng::fpng_init();

        if (!reshade::register_addon(hModule))
            return FALSE;

        screenshot_environment env(nullptr);
        env.init();

        reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init);
        reshade::register_event<reshade::addon_event::destroy_device>(on_destroy);
        reshade::register_event<reshade::addon_event::present>(on_device_present);
        reshade::register_event<reshade::addon_event::reshade_begin_effects>(on_begin_effects);
        reshade::register_event<reshade::addon_event::reshade_finish_effects>(on_finish_effects);
        reshade::register_event<reshade::addon_event::reshade_overlay>(on_reshade_overlay);
        reshade::register_event<reshade::addon_event::reshade_present>(on_reshade_present);
        reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(on_reshade_reloaded_effects);
        reshade::register_overlay("OSD", draw_osd_window);
        reshade::register_overlay("Settings###settings", draw_setting_window);
    }
    else if (fdwReason == DLL_PROCESS_DETACH)
    {
        reshade::unregister_overlay("OSD", draw_osd_window);
        reshade::unregister_overlay("Settings###settings", draw_setting_window);
        reshade::unregister_addon(hModule);

        g_module_handle = nullptr;
    }

    return TRUE;
}

