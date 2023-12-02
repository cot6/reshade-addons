/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <imgui.h>
#include <reshade.hpp>

#include "dllmain.hpp"
#include "imgui_widgets.hpp"
#include "runtime_config.hpp"

#include <fpng.h>

#include <filesystem>
#include <list>
#include <string>
#include <thread>

using namespace reshade::api;

HMODULE g_module_handle;

void screenshot_context::save()
{
    config.save(ini_file::load_cache(environment.reshade_base_path / "ReShade_Addon_Screenshot.ini"), false);
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
    if (active_screenshot->repeat_wait != 0 && (screenshot_current_frame - screenshot_begin_frame) % active_screenshot->repeat_wait)
        return false;

    return true;
}
inline bool screenshot_context::is_screenshot_frame(screenshot_kind kind) const noexcept
{
    return is_screenshot_frame() && active_screenshot->is_enable(kind);
}

static void on_init(reshade::api::effect_runtime *runtime)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->create_private_data<screenshot_context>();

    ctx.active_screenshot = nullptr;

    ctx.environment.load(runtime);
    ctx.config.load(ini_file::load_cache(ctx.environment.reshade_base_path / "ReShade_Addon_Screenshot.ini"));

    ctx.screenshot_begin_frame = std::numeric_limits<decltype(ctx.screenshot_begin_frame)>::max();
}
static void on_destroy(reshade::api::device *device)
{
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

    ctx.screenshot_current_frame++;
    ctx.present_time = std::chrono::steady_clock::now();

    if (ctx.is_screenshot_frame(screenshot_kind::original))
    {
        screenshot &screenshot = ctx.screenshots.emplace_front(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::original, ctx.screenshot_state);
        screenshot.repeat_index = ctx.screenshot_repeat_index;
    }

    if (ctx.is_screenshot_frame(screenshot_kind::depth) &&
        ctx.screenshotdepth_technique.handle == 0)
        ctx.screenshotdepth_technique = runtime->find_technique("__Addon_ScreenshotDepth_Seri14.fx", "__Addon_Technique_ScreenshotDepth_Seri14");

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
static void on_reloaded_effects(reshade::api::effect_runtime *runtime)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();

    if (ctx.screenshotdepth_technique = runtime->find_technique("__Addon_ScreenshotDepth_Seri14.fx", "__Addon_Technique_ScreenshotDepth_Seri14");
        ctx.screenshotdepth_technique.handle == 0)
    {
        size_t size = 0;
        runtime->get_current_preset_path(nullptr, &size);
        std::string path; path.resize(size + 1);
        runtime->get_current_preset_path(path.data(), &size);
        path.resize(size);

        if (ini_file preset(path); preset.has({}, "Techniques"))
        {
            std::vector<std::string> technique_list;
            preset.get({}, "Techniques", technique_list);

            const std::string_view technique_name = "__Addon_Technique_ScreenshotDepth_Seri14@__Addon_ScreenshotDepth_Seri14.fx";
            if (std::find_if(technique_list.cbegin(), technique_list.cend(),
                [&technique_name](const std::string &technique) { return technique == technique_name; }) == technique_list.cend())
            {
                technique_list.emplace_back(technique_name);
                preset.set({}, "Techniques", technique_list);

                runtime->set_preprocessor_definition_for_effect(nullptr, "__Addon_Technique_ScreenshotDepth_Seri14", nullptr);
            }
        }
    }
}
static void on_begin_effects(reshade::api::effect_runtime *runtime, reshade::api::command_list *, reshade::api::resource_view, reshade::api::resource_view)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();

    if (ctx.is_screenshot_frame(screenshot_kind::before))
    {
        screenshot &screenshot = ctx.screenshots.emplace_front(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::before, ctx.screenshot_state);
        screenshot.repeat_index = ctx.screenshot_repeat_index;
    }
}
static void on_finish_effects(reshade::api::effect_runtime *runtime, reshade::api::command_list *, reshade::api::resource_view, reshade::api::resource_view)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();

    if (ctx.is_screenshot_frame(screenshot_kind::after))
    {
        screenshot &screenshot = ctx.screenshots.emplace_front(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::after, ctx.screenshot_state);
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

        if (reshade::api::effect_texture_variable texture = runtime->find_texture_variable("__Addon_ScreenshotDepth_Seri14.fx", "__Addon_Texture_ScreenshotDepth_Seri14");
            texture.handle != 0)
        {
            reshade::api::resource_view rsv{}, rsv_srgb{};
            runtime->get_texture_binding(texture, &rsv, &rsv_srgb);
            if (reshade::api::resource resource = device->get_resource_from_view(rsv);
                resource.handle != 0)
            {
                std::vector<uint8_t> texture_data;
                reshade::api::format texture_format;
                if (get_texture_data(resource, reshade::api::resource_usage::shader_resource, texture_data, texture_format))
                {
                    screenshot &screenshot = ctx.screenshots.emplace_front(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::depth, ctx.screenshot_state);
                    screenshot.repeat_index = ctx.screenshot_repeat_index;
                    screenshot.pixels = std::move(texture_data);
                }
            }
        }
    }
}
static void on_reshade_present(reshade::api::effect_runtime *runtime)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();

    if (ctx.is_screenshot_frame(screenshot_kind::overlay))
    {
        screenshot &screenshot = ctx.screenshots.emplace_front(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::overlay, ctx.screenshot_state);
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
        ctx.screenshot_repeat_index++;

    if (ctx.active_screenshot != nullptr && ctx.active_screenshot->repeat_count != 0 && ctx.active_screenshot->repeat_count <= ctx.screenshot_repeat_index)
    {
        if (ctx.screenshotdepth_technique.handle != 0 &&
            runtime->get_technique_state(ctx.screenshotdepth_technique) != false)
            runtime->set_technique_state(ctx.screenshotdepth_technique, false);

        if (ctx.config.turn_on_effects == decltype(screenshot_config::turn_on_effects)::turn_on_while_myset_is_active && !ctx.before_screenshot_enable_effects &&
            ctx.is_screenshot_enable(screenshot_kind::after) &&
            runtime->get_effects_state())
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
                if (ctx.active_screenshot == &screenshot_myset)
                {
                    ctx.active_screenshot = nullptr;
                }
                else
                {
                    ctx.screenshot_state.reset();
                    ctx.active_screenshot = &screenshot_myset;

                    ctx.before_screenshot_enable_effects = runtime->get_effects_state();
                    ctx.screenshot_begin_frame = ctx.screenshot_current_frame + 1;
                    ctx.screenshot_repeat_index = 0;

                    if (screenshot_myset.worker_threads != 0)
                        ctx.screenshot_worker_threads = screenshot_myset.worker_threads;
                    else
                        ctx.screenshot_worker_threads = ctx.environment.thread_hardware_concurrency;

                    if (ctx.is_screenshot_enable(screenshot_kind::depth) &&
                        ctx.screenshotdepth_technique.handle == 0)
                        ctx.screenshotdepth_technique = runtime->find_technique("__Addon_ScreenshotDepth_Seri14.fx", "__Addon_Technique_ScreenshotDepth_Seri14");

                    if (ctx.screenshotdepth_technique.handle != 0 &&
                        runtime->get_technique_state(ctx.screenshotdepth_technique) == false)
                        runtime->set_technique_state(ctx.screenshotdepth_technique, true);

                    if ((ctx.config.turn_on_effects == decltype(screenshot_config::turn_on_effects)::turn_on_when_activate_myset) &&
                        ctx.is_screenshot_enable(screenshot_kind::after) &&
                        !runtime->get_effects_state())
                        runtime->set_effects_state(true);

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

    // Disable keyboard shortcuts while typing into input boxes
    ctx.ignore_shortcuts = ImGui::IsAnyItemActive();
}

static const ImVec4 COLOR_RED = ImColor(240, 100, 100);
static const ImVec4 COLOR_YELLOW = ImColor(204, 204, 0);

static void draw_osd_window(reshade::api::effect_runtime *runtime)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();

    switch (ctx.config.show_osd)
    {
        case decltype(screenshot_config::show_osd)::hidden:
            return;
        case decltype(screenshot_config::show_osd)::show_osd_always:
            break;
        case decltype(screenshot_config::show_osd)::show_osd_while_myset_is_active:
            if (ctx.active_screenshot == nullptr)
                return;
            break;
    }

    ImGui::TextUnformatted("Active set: ");
    if (ctx.active_screenshot)
    {
        ImGui::SameLine();
        ImGui::TextUnformatted(ctx.active_screenshot->name.c_str(), ctx.active_screenshot->name.c_str() + ctx.active_screenshot->name.size());
    }

    float fraction;
    std::string str;
    if (ctx.active_screenshot != nullptr)
    {
        fraction = ctx.active_screenshot->repeat_count != 0 ? (static_cast<float>(ctx.screenshot_repeat_index + 1) / ctx.active_screenshot->repeat_count) : 1.0f;
        str = std::format(ctx.active_screenshot->repeat_count != 0 ? "%u of %u" : "%u times (Infinite mode)", (ctx.screenshot_repeat_index), ctx.active_screenshot->repeat_count);
    }
    else
    {
        fraction = 1.0f;
        str = "ready";
    }
    ImGui::ProgressBar(1.0f, ImVec2(ImGui::GetContentRegionAvail().x, 0), "");
    ImGui::SameLine(15);
    ImGui::TextUnformatted(str.c_str(), str.c_str() + str.size());

    uint64_t using_bytes = 0;
    std::for_each(ctx.screenshots.cbegin(), ctx.screenshots.cend(),
        [&using_bytes](const screenshot &screenshot) {
            using_bytes += screenshot.pixels.size();
        });
    str = std::format("%u shots in queue (%.3lf MiB)", ctx.screenshots.size(), static_cast<double>(using_bytes) / (1024 * 1024 * 1));
    ImGui::TextUnformatted(str.c_str(), str.c_str() + str.size());
    if (ctx.active_screenshot != nullptr && ctx.screenshot_state.error_occurs > 0)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR_RED);
        ImGui::TextUnformatted("Some errors occurred. Check the log for more details.");
        ImGui::PopStyleColor();
    }
    if ((ctx.is_screenshot_enable(screenshot_kind::before) || ctx.is_screenshot_enable(screenshot_kind::after)) &&
        (ctx.config.turn_on_effects == decltype(screenshot_config::turn_on_effects)::ignore || ctx.config.turn_on_effects == decltype(screenshot_config::turn_on_effects)::turn_on_when_activate_myset) &&
        !runtime->get_effects_state())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR_YELLOW);
        ImGui::TextUnformatted("[WARNING] Skipping  \"Before\" and \"After\" captures because effects are disabled.");
        ImGui::PopStyleColor();
    }
    if (ctx.is_screenshot_enable(screenshot_kind::depth) &&
        (ctx.config.turn_on_effects == decltype(screenshot_config::turn_on_effects)::ignore || ctx.config.turn_on_effects == decltype(screenshot_config::turn_on_effects)::turn_on_when_activate_myset) &&
        !runtime->get_effects_state())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR_YELLOW);
        ImGui::TextUnformatted("[WARNING] Skipping  \"Depth\" capture because effects are disabled.");
        ImGui::PopStyleColor();
    }
    if (ctx.is_screenshot_enable(screenshot_kind::depth) &&
        runtime->get_effects_state() &&
        ctx.screenshotdepth_technique.handle == 0)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR_RED);
        ImGui::TextUnformatted("[BUGCHECK] \"Depth\" capture cannot be performed.");
        ImGui::PopStyleColor();
    }
}
static void draw_setting_window(reshade::api::effect_runtime *runtime)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();
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

    if (ImGui::CollapsingHeader("Screenshot Add-On [by seri14]", ImGuiTreeNodeFlags_DefaultOpen))
    {
        modified |= ImGui::Combo("Show OSD", reinterpret_cast<int *>(&ctx.config.show_osd), "Hidden\0Always\0While myset is active\0");
        modified |= ImGui::Combo("Turn On Effects", reinterpret_cast<int *>(&ctx.config.turn_on_effects), "Ignore\0While myset is active\0When activate myset\0");

        char buf[4096] = "";

        for (screenshot_myset &screenshot_myset : ctx.config.screenshot_mysets)
        {
            ImGui::PushID(screenshot_myset.name.c_str(), screenshot_myset.name.c_str() + screenshot_myset.name.size());

            if (ImGui::CollapsingHeader(screenshot_myset.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto path_filter = [](ImGuiInputTextCallbackData *data) -> int
                    {
                        static const std::wstring_view invalid_characters = L"[]\"/|?*";
                        return 0x0 <= data->EventChar && data->EventChar <= 0x1F || data->EventChar == 0x7F || invalid_characters.find(data->EventChar) != std::string::npos;
                    };
                modified |= reshade::imgui::key_input_box("Screenshot key", screenshot_myset.screenshot_key_data, runtime);
                bool isItemHovered = false;
                if (buf[screenshot_myset.original_image.u8string().copy(buf, sizeof(buf) - 1)] = '\0';
                    ImGui::InputTextWithHint("Original image", "Enter path to enable", buf, sizeof(buf), ImGuiInputTextFlags_CallbackCharFilter, path_filter))
                {
                    modified = true;
                    screenshot_myset.original_image = std::filesystem::u8path(buf);
                }
                if (!isItemHovered)
                    isItemHovered = ImGui::IsItemHovered();
                if (buf[screenshot_myset.before_image.u8string().copy(buf, sizeof(buf) - 1)] = '\0';
                    ImGui::InputTextWithHint("Before image", "Enter path to enable", buf, sizeof(buf), ImGuiInputTextFlags_CallbackCharFilter, path_filter))
                {
                    modified = true;
                    screenshot_myset.before_image = std::filesystem::u8path(buf);
                }
                if (!isItemHovered)
                    isItemHovered = ImGui::IsItemHovered();
                if (buf[screenshot_myset.after_image.u8string().copy(buf, sizeof(buf) - 1)] = '\0';
                    ImGui::InputTextWithHint("After image", "Enter path to enable", buf, sizeof(buf), ImGuiInputTextFlags_CallbackCharFilter, path_filter))
                {
                    modified = true;
                    screenshot_myset.after_image = std::filesystem::u8path(buf);
                }
                if (!isItemHovered)
                    isItemHovered = ImGui::IsItemHovered();
                if (buf[screenshot_myset.overlay_image.u8string().copy(buf, sizeof(buf) - 1)] = '\0';
                    ImGui::InputTextWithHint("Overlay image", "Enter path to enable", buf, sizeof(buf), ImGuiInputTextFlags_CallbackCharFilter, path_filter))
                {
                    modified = true;
                    screenshot_myset.overlay_image = std::filesystem::u8path(buf);
                }
                if (!isItemHovered)
                    isItemHovered = ImGui::IsItemHovered();
                if (buf[screenshot_myset.depth_image.u8string().copy(buf, sizeof(buf) - 1)] = '\0';
                    ImGui::InputTextWithHint("Depth image", "Enter path to enable", buf, sizeof(buf), ImGuiInputTextFlags_CallbackCharFilter, path_filter))
                {
                    modified = true;
                    screenshot_myset.depth_image = std::filesystem::u8path(buf);
                }
                if (!isItemHovered)
                    isItemHovered = ImGui::IsItemHovered();
                if (isItemHovered)
                {
                    ImGui::SetTooltip(
                        "Macros you can add that are resolved during saving:\n"
                        "  <APP>               File name of the current executable file (%s)\n"
                        "  <PRESET>            File name of the current preset file (%s)\n"
                        "  <INDEX[:format]>    Current number of continuous screenshot\n"
                        "                      (default: D1)\n"
                        "  <DATE[:format]>     Timestamp of taken screenshot\n"
                        "                      (default: %%Y-%%m-%%d %%H-%%M-%%S)",
                        ctx.environment.reshade_executable_path.stem().string().c_str(),
                        ctx.environment.reshade_preset_path.stem().string().c_str());
                }
                modified |= ImGui::Combo("File format", reinterpret_cast<int *>(&screenshot_myset.image_format),
                    "[libpng] 24-bit PNG\0"
                    "[libpng] 32-bit PNG\0"
                    "[fpng] 24-bit PNG\0"
                    "[fpng] 32-bit PNG\0"
                    "[libtiff] 24-bit TIFF\0"
                    "[libtiff] 32-bit TIFF\0"
                );
                if (ImGui::DragInt("Repeat count", reinterpret_cast<int *>(&screenshot_myset.repeat_count), 1.0f, 0, 0, screenshot_myset.repeat_count == 0 ? "infinity" : "%d"))
                {
                    if (static_cast<int>(screenshot_myset.repeat_count) < 0)
                        screenshot_myset.repeat_count = 1;

                    modified = true;
                }
                if (ImGui::DragInt("Repeat wait", reinterpret_cast<int *>(&screenshot_myset.repeat_wait), 1.0f, 1, 0, "%d"))
                {
                    if (static_cast<int>(screenshot_myset.repeat_wait) < 1)
                        screenshot_myset.repeat_wait = 1;

                    modified = true;
                }
                if (ImGui::DragInt("Worker threads", reinterpret_cast<int *>(&screenshot_myset.worker_threads), 1.0f, 0, ctx.environment.thread_hardware_concurrency, screenshot_myset.worker_threads == 0 ? "unlimited" : "%d"))
                {
                    if (static_cast<int>(screenshot_myset.worker_threads) < 0)
                        screenshot_myset.worker_threads = 1;

                    modified = true;
                }
                uint32_t width = 0, height = 0;
                runtime->get_screenshot_width_and_height(&width, &height);
                int enables = 0, depths = 0;
                if (screenshot_myset.is_enable(screenshot_kind::original)) { enables += 1; depths += 4; }
                if (screenshot_myset.is_enable(screenshot_kind::before)) { enables += 1; depths += 4; }
                if (screenshot_myset.is_enable(screenshot_kind::after)) { enables += 1; depths += 4; }
                if (screenshot_myset.is_enable(screenshot_kind::overlay)) { enables += 1; depths += 4; }
                if (screenshot_myset.is_enable(screenshot_kind::depth)) { enables += 1; depths += 2; }
                ImGui::Text("Estimate memory usage: %.3lf MiB per once (%d images)", static_cast<double>(depths * width * height) / (1024 * 1024 * 1), enables);
            }

            ImGui::PopID();
        }
        ImGui::TextUnformatted("Add set:");
        ImGui::SameLine();
        if (ImGui::Button("+"))
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
        reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(on_reloaded_effects);
        reshade::register_event<reshade::addon_event::reshade_begin_effects>(on_begin_effects);
        reshade::register_event<reshade::addon_event::reshade_finish_effects>(on_finish_effects);
        reshade::register_event<reshade::addon_event::reshade_overlay>(on_reshade_overlay);
        reshade::register_event<reshade::addon_event::reshade_present>(on_reshade_present);
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

