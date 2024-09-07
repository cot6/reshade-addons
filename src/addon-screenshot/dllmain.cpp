/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <windows.h>
#include <mmsystem.h>

#include <imgui.h>
#include <reshade.hpp>
#include <forkawesome.h>

#include "addon.hpp"
#include "std_string_ext.hpp"

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
    config.save(ini_file::load_cache(environment.addon_screenshot_config_path), false);
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
    ctx.statistics.load(ini_file::load_cache(ctx.environment.addon_screenshot_statistics_path));

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

        if (ctx.statistics.capture_counts.try_emplace({}).first->second.total_frame++; ctx.active_screenshot != nullptr)
            ctx.statistics.capture_counts.try_emplace(ctx.active_screenshot->name).first->second.total_frame++;

        ctx.statistics.save(ini_file::load_cache(ctx.environment.addon_screenshot_statistics_path));
    }

    if (ctx.is_screenshot_frame(screenshot_kind::original))
    {
        screenshot &screenshot = ctx.screenshots.emplace_back(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::original, ctx.screenshot_state, ctx.present_time, ctx.statistics);
        screenshot.repeat_index = ctx.screenshot_repeat_index;
    }

    if (reshade::api::effect_technique technique = runtime->find_technique("__Addon_ScreenshotDepth_Seri14.addonfx", "__Addon_Technique_ScreenshotDepth_Seri14"); technique.handle != 0)
    {
        const bool enabled = ctx.is_screenshot_frame(screenshot_kind::depth);

        if (runtime->get_technique_state(technique) != enabled)
            runtime->set_technique_state(technique, enabled);
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
        screenshot &screenshot = ctx.screenshots.emplace_back(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::before, ctx.screenshot_state, ctx.present_time, ctx.statistics);
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
        screenshot &screenshot = ctx.screenshots.emplace_back(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::after, ctx.screenshot_state, ctx.present_time, ctx.statistics);
        screenshot.repeat_index = ctx.screenshot_repeat_index;
    }

    if (ctx.is_screenshot_frame(screenshot_kind::depth) &&
        runtime->find_technique("__Addon_ScreenshotDepth_Seri14.addonfx", "__Addon_Technique_ScreenshotDepth_Seri14").handle != 0)
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
                        screenshot &screenshot = ctx.screenshots.emplace_back(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::depth, ctx.screenshot_state, ctx.present_time, ctx.statistics);
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
        screenshot &screenshot = ctx.screenshots.emplace_back(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::overlay, ctx.screenshot_state, ctx.present_time, ctx.statistics);
        screenshot.repeat_index = ctx.screenshot_repeat_index;
    }

    if (!ctx.screenshots.empty())
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

                ctx.statistics.save(ini_file::load_cache(ctx.environment.addon_screenshot_statistics_path));

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

                    if (ctx.statistics.capture_counts.try_emplace({}).first->second.total_take++; ctx.active_screenshot != nullptr)
                        ctx.statistics.capture_counts.try_emplace(ctx.active_screenshot->name).first->second.total_take++;

                    ctx.statistics.save(ini_file::load_cache(ctx.environment.addon_screenshot_statistics_path));

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
            static_cast<long>(ctx.screenshot_state.last_elapsed / std::max<size_t>(1, ctx.screenshot_worker_threads)) > (ctx.capture_time - ctx.capture_last).count())
            ImGui::TextColored(COLOR_YELLOW, "%s", _("Processing of screenshots is too slow!"));

        if (ctx.screenshot_state.error_occurs > 0)
            ImGui::TextColored(COLOR_RED, "%s", _("Failed. Check details in the log."));

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

    if (runtime->find_technique("__Addon_ScreenshotDepth_Seri14.addonfx", "__Addon_Technique_ScreenshotDepth_Seri14").handle == 0)
        ImGui::TextColored(COLOR_RED, "%s", _("[BUGCHECK] \"Depth\" capture cannot be performed."));
}
static void draw_setting_window(reshade::api::effect_runtime *runtime)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();
    if (std::addressof(ctx) == nullptr)
        return;

    const float button_size = ImGui::GetFrameHeight();
    const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

    bool modified = false;

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

                screenshot dummy(nullptr, ctx.environment, screenshot_myset, kind, ctx.screenshot_state, ctx.present_time, ctx.statistics);
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

                ImGui::BeginTabBar("##screenshot_path_settings", ImGuiTabBarFlags_None);

                bool tab_open = ImGui::BeginTabItem(_("Screenshot path"), nullptr, ImGuiTabItemFlags_None);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::TextUnformatted(_("Set save paths for screenshots."));
                        ImGui::EndTooltip();
                    }
                }
                if (tab_open)
                {
                    const auto draw_control_screenshot_path = [&](screenshot_kind kind, std::filesystem::path &path, std::string &status, const char *text_id, const char *enabled_id, unsigned short resource_id)
                        {
                            if (!status.empty())
                            {
                                ImGui::PushStyleColor(ImGuiCol_Text, COLOR_RED);
                                ImGui::TextUnformatted(status.data(), status.data() + status.size());
                                ImGui::PopStyleColor();
                            }

                            buf[path.u8string().copy(buf, sizeof(buf) - 1)] = '\0';

                            ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - button_size - button_spacing);
                            ImGui::InputTextWithHint(text_id, _("Enter path to capture"), buf, sizeof(buf), ImGuiInputTextFlags_CallbackCharFilter, path_filter);
                            bool item_hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip);

                            if (ImGui::IsItemDeactivatedAfterEdit())
                            {
                                modified = true;
                                path = std::filesystem::u8path(buf);
                                validate_image_path(kind, path, status);
                            }

                            if (!path.empty())
                            {
                                ImGui::SameLine(0, button_spacing);
                                if (bool v = !screenshot_myset.is_muted(kind); ImGui::Checkbox(enabled_id, &v))
                                {
                                    modified = true;
                                    if (!v)
                                        screenshot_myset.mute(kind);
                                    else
                                        screenshot_myset.unmute(kind);
                                }
                                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                                {
                                    if (ImGui::BeginTooltip())
                                    {
                                        ImGui::TextUnformatted(_("Toggle whether to capture the frame."));
                                        ImGui::EndTooltip();
                                    }
                                }
                            }
                            else
                            {
                                ImGui::SameLine(0, button_spacing);
                                ImGui::Dummy(ImVec2(button_size, 0));
                            }

                            ImGui::SameLine(0, button_spacing);
                            std::string resource = reshade::resources::load_string(resource_id);
                            ImGui::TextUnformatted(resource.c_str(), resource.c_str() + resource.size());
                            item_hovered |= ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip);

                            if (item_hovered)
                            {
                                if (ImGui::BeginTooltip())
                                {
                                    std::string tooltip;
                                    switch (kind)
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
                                        "  <TOTALFRAME[:format]>\n"
                                        "                      Total number of screenshots\n"
                                        "                      (default: D1)\n"
                                        "  <MYSETFRAME[:format]>\n"
                                        "                      Total number of screenshots of myset\n"
                                        "                      (default: D1)\n"
                                        "  <TOTALTAKE[:format]>\n"
                                        "                      Total number of activation\n"
                                        "                      (default: D1)\n"
                                        "  <MYSETTAKE[:format]>\n"
                                        "                      Total number of activation of myset\n"
                                        "                      (default: D1)\n"
                                        "  <INDEX[:format]>    Current number of continuous screenshot\n"
                                        "                      (default: D1)\n"
                                        "  <DATE[:format]>     Timestamp of taken screenshot\n"
                                        "                      (default: %%Y-%%m-%%d %%H-%%M-%%S)");
                                    ImGui::Text(tooltip.c_str(),
                                        ctx.environment.reshade_executable_path.stem().string().c_str(),
                                        ctx.environment.reshade_preset_path.stem().string().c_str());
                                    ImGui::EndTooltip();
                                }
                            }
                        };

                    draw_control_screenshot_path(screenshot_kind::original, screenshot_myset.original_image, screenshot_myset.original_status, "##image_original", "##image_original_enabled", __COMPUTE_CRC16("Original image"));
                    draw_control_screenshot_path(screenshot_kind::before, screenshot_myset.before_image, screenshot_myset.before_status, "##image_before", "##image_before_enabled", __COMPUTE_CRC16("Before image"));
                    draw_control_screenshot_path(screenshot_kind::after, screenshot_myset.after_image, screenshot_myset.after_status, "##image_after", "##image_after_enabled", __COMPUTE_CRC16("After image"));
                    draw_control_screenshot_path(screenshot_kind::overlay, screenshot_myset.overlay_image, screenshot_myset.overlay_status, "##image_overlay", "##image_overlay_enabled", __COMPUTE_CRC16("Overlay image"));
                    draw_control_screenshot_path(screenshot_kind::depth, screenshot_myset.depth_image, screenshot_myset.depth_status, "##image_depth", "##image_depth_enabled", __COMPUTE_CRC16("Depth image"));

                    ImGui::EndTabItem();
                }

                tab_open = ImGui::BeginTabItem(_("Free space limit"), nullptr, ImGuiTabItemFlags_None);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::TextUnformatted(_("Set thresholds to block taking screenshots to preserve the free disk space."));
                        ImGui::EndTooltip();
                    }
                }
                if (tab_open)
                {
                    auto select_format = [](uint64_t size) -> std::string { return (size == 0 || size == std::numeric_limits<decltype(size)>::max()) ? _("Disabled") : size >= 100 ? "%d MB" : "%d %%"; };

                    screenshot_kind screenshot_path_hovered = screenshot_kind::unset;
                    if (int v = static_cast<int>(screenshot_myset.original_freelimit);
                        ImGui::DragInt(_("Original image"), &v, 1, 0, 100, select_format(screenshot_myset.original_freelimit).c_str(), ImGuiSliderFlags_None))
                    {
                        modified = true;
                        screenshot_myset.original_freelimit = static_cast<decltype(screenshot_myset.original_freelimit)>(v);
                    }
                    if (screenshot_path_hovered == screenshot_kind::unset && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                        screenshot_path_hovered = screenshot_kind::original;
                    if (int v = static_cast<int>(screenshot_myset.before_freelimit);
                        ImGui::DragInt(_("Before image"), &v, 1, 0, 100, select_format(screenshot_myset.before_freelimit).c_str(), ImGuiSliderFlags_None))
                    {
                        modified = true;
                        screenshot_myset.before_freelimit = static_cast<decltype(screenshot_myset.before_freelimit)>(v);
                    }
                    if (screenshot_path_hovered == screenshot_kind::unset && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                        screenshot_path_hovered = screenshot_kind::before;
                    if (int v = static_cast<int>(screenshot_myset.after_freelimit);
                        ImGui::DragInt(_("After image"), &v, 1, 0, 100, select_format(screenshot_myset.after_freelimit).c_str(), ImGuiSliderFlags_None))
                    {
                        modified = true;
                        screenshot_myset.after_freelimit = static_cast<decltype(screenshot_myset.after_freelimit)>(v);
                    }
                    if (screenshot_path_hovered == screenshot_kind::unset && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                        screenshot_path_hovered = screenshot_kind::after;
                    if (int v = static_cast<int>(screenshot_myset.overlay_freelimit);
                        ImGui::DragInt(_("Overlay image"), &v, 1, 0, 100, select_format(screenshot_myset.overlay_freelimit).c_str(), ImGuiSliderFlags_None))
                    {
                        modified = true;
                        screenshot_myset.overlay_freelimit = static_cast<decltype(screenshot_myset.overlay_freelimit)>(v);
                    }
                    if (screenshot_path_hovered == screenshot_kind::unset && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                        screenshot_path_hovered = screenshot_kind::overlay;
                    if (int v = static_cast<int>(screenshot_myset.depth_freelimit);
                        ImGui::DragInt(_("Depth image"), &v, 1, 0, 100, select_format(screenshot_myset.depth_freelimit).c_str(), ImGuiSliderFlags_None))
                    {
                        modified = true;
                        screenshot_myset.depth_freelimit = static_cast<decltype(screenshot_myset.depth_freelimit)>(v);
                    }
                    if (screenshot_path_hovered == screenshot_kind::unset && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                        screenshot_path_hovered = screenshot_kind::depth;
                    if (screenshot_path_hovered != screenshot_kind::unset)
                    {
                        if (ImGui::BeginTooltip())
                        {
                            ImGui::TextUnformatted(_("This feature estimates the free disk space in the destination folder before saving screenshots to cancel the save operation.\n"
                                "Note that this behavior depends on the following ranges.\n"
                                "0       Disable feature\n"
                                "1-99    Percentage of free disk space to cancel the save operation.\n"
                                "100-    Number of free disk space (in megabytes) to cancel the save operation.\n\n"
                                "If you want to specify a value above 100 MB, you can switch to direct input mode by 'Ctrl'+left click."));

                            ImGui::EndTooltip();
                        }
                    }

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
                ImGui::Dummy(ImVec2());

                if (ImGui::SliderInt(_("Repeat count"), reinterpret_cast<int *>(&screenshot_myset.repeat_count), 0, 60, screenshot_myset.repeat_count == 0 ? _("infinity") : _("%d times"), ImGuiSliderFlags_None))
                {
                    if (static_cast<int>(screenshot_myset.repeat_count) < 0)
                        screenshot_myset.repeat_count = 1;

                    modified = true;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::TextUnformatted(_("Specify the number of frames to save after pressing the screenshot shortcut key. To abort capture, re-enter the screenshot key."));
                        ImGui::EndTooltip();
                    }
                }
                if (ImGui::SliderInt(_("Repeat interval"), reinterpret_cast<int *>(&screenshot_myset.repeat_interval), 1, 60, screenshot_myset.repeat_interval > 1 ? _("%d frames") : _("every frame"), ImGuiSliderFlags_None))
                {
                    if (static_cast<int>(screenshot_myset.repeat_interval) < 1)
                        screenshot_myset.repeat_interval = 1;

                    modified = true;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::TextUnformatted(_("Specify the interval of frames to be save until the specified number of frames."));
                        ImGui::EndTooltip();
                    }
                }
                if (ImGui::SliderInt(_("Worker threads"), reinterpret_cast<int *>(&screenshot_myset.worker_threads), 0, std::thread::hardware_concurrency(), screenshot_myset.worker_threads == 0 ? _("unlimited") : _("%d threads")))
                {
                    if (static_cast<int>(screenshot_myset.worker_threads) < 0)
                        screenshot_myset.worker_threads = 1;

                    modified = true;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::TextUnformatted(_("Specify the number of threads to compress the captured frames to specified image files."));
                        ImGui::EndTooltip();
                    }
                }
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
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::TextUnformatted(_("Select the image file format.\nHowever, the depth is always saved in TIFF format regardless of this selection."));
                        ImGui::EndTooltip();
                    }
                }
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
        reshade::register_overlay("OSD", draw_osd_window);
        reshade::register_overlay("Settings###settings", draw_setting_window);

        ini_file::flush_cache(true);
    }
    else if (fdwReason == DLL_PROCESS_DETACH)
    {
        ini_file::flush_cache(true);

        reshade::unregister_overlay("OSD", draw_osd_window);
        reshade::unregister_overlay("Settings###settings", draw_setting_window);
        reshade::unregister_addon(hModule);

        g_module_handle = nullptr;
    }

    return TRUE;
}

