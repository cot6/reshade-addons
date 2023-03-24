/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <imgui.h>
#include <reshade.hpp>

#include "dllmain.hpp"
#include "imgui_widgets.hpp"
#include "runtime_config.hpp"

#include <zlib.h>
#include <png.h>

#include <filesystem>
#include <list>
#include <string>
#include <thread>

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

    reshade::api::effect_runtime *runtime = static_cast<reshade::api::effect_runtime *>(swapchain);
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();

    ctx.screenshot_current_frame++;
    ctx.present_time = std::chrono::steady_clock::now();

    if (ctx.is_screenshot_frame(screenshot_kind::original))
    {
        screenshot &screenshot = ctx.screenshots.emplace_front(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::original);
        screenshot.repeat_index = ctx.screenshot_repeat_index;
    }
}
static void on_begin_effects(reshade::api::effect_runtime *runtime, reshade::api::command_list *, reshade::api::resource_view, reshade::api::resource_view)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();

    if (ctx.is_screenshot_frame(screenshot_kind::before))
    {
        screenshot &screenshot = ctx.screenshots.emplace_front(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::before);
        screenshot.repeat_index = ctx.screenshot_repeat_index;
    }
}
static void on_finish_effects(reshade::api::effect_runtime *runtime, reshade::api::command_list *, reshade::api::resource_view, reshade::api::resource_view)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();

    if (ctx.is_screenshot_frame(screenshot_kind::after))
    {
        screenshot &screenshot = ctx.screenshots.emplace_front(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::after);
        screenshot.repeat_index = ctx.screenshot_repeat_index;
    }
}
static void on_reshade_present(reshade::api::effect_runtime *runtime)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();

    if (ctx.is_screenshot_frame(screenshot_kind::overlay))
    {
        screenshot &screenshot = ctx.screenshots.emplace_front(runtime, ctx.environment, *ctx.active_screenshot, screenshot_kind::overlay);
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
        ctx.active_screenshot = nullptr;

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
                    ctx.active_screenshot = &screenshot_myset;
                    ctx.screenshot_begin_frame = ctx.screenshot_current_frame + 1;
                    ctx.screenshot_repeat_index = 0;
                    if (screenshot_myset.worker_threads != 0)
                        ctx.screenshot_worker_threads = screenshot_myset.worker_threads;
                    else
                        ctx.screenshot_worker_threads = ctx.environment.thread_hardware_concurrency;
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

static void draw_osd_window(reshade::api::effect_runtime *runtime)
{
    reshade::api::device *device = runtime->get_device();
    screenshot_context &ctx = device->get_private_data<screenshot_context>();

    switch (ctx.config.show_osd)
    {
        case decltype(screenshot_config::show_osd)::hidden:
            return;
        case decltype(screenshot_config::show_osd)::always:
            break;
        case decltype(screenshot_config::show_osd)::while_myset_is_active:
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

    if (ImGui::CollapsingHeader("Screenshot [seri14's Add-On]", ImGuiTreeNodeFlags_DefaultOpen))
    {
        modified |= ImGui::Combo("Show OSD", reinterpret_cast<int *>(&ctx.config.show_osd), "Hidden\0Always\0While myset is active\0");

        char buf[4096];

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
                modified |= ImGui::Combo("File format", reinterpret_cast<int *>(&screenshot_myset.image_format), "24-bit PNG (RGB8)\0" "32-bit PNG (RGBA8)\0");
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
                int enables = 0;
                enables += screenshot_myset.is_enable(screenshot_kind::original) ? 1 : 0;
                enables += screenshot_myset.is_enable(screenshot_kind::before) ? 1 : 0;
                enables += screenshot_myset.is_enable(screenshot_kind::after) ? 1 : 0;
                enables += screenshot_myset.is_enable(screenshot_kind::overlay) ? 1 : 0;
                ImGui::Text("Estimate memory usage: %.3lf MiB per once (%d images)", static_cast<double>(4 * width * height) / (1024 * 1024 * 1) * enables, enables);
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
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            if (!reshade::register_addon(hModule))
                return FALSE;
            reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init);
            reshade::register_event<reshade::addon_event::destroy_device>(on_destroy);
            reshade::register_event<reshade::addon_event::present>(on_device_present);
            reshade::register_event<reshade::addon_event::reshade_begin_effects>(on_begin_effects);
            reshade::register_event<reshade::addon_event::reshade_finish_effects>(on_finish_effects);
            reshade::register_event<reshade::addon_event::reshade_overlay>(on_reshade_overlay);
            reshade::register_event<reshade::addon_event::reshade_present>(on_reshade_present);
            reshade::register_overlay("OSD", draw_osd_window);
            reshade::register_overlay("Settings", draw_setting_window);
            break;
        case DLL_PROCESS_DETACH:
            reshade::unregister_overlay("OSD", draw_osd_window);
            reshade::unregister_overlay("Settings", draw_setting_window);
            reshade::unregister_addon(hModule);
            break;
    }

    return TRUE;
}

