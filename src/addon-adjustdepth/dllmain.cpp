/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <imgui.h>
#include <reshade.hpp>
#include <forkawesome.h>

#include "dllmain.hpp"
#include "imgui_widgets.hpp"
#include "input.hpp"
#include "localization.hpp"

#include "std_string_ext.hpp"

#include <chrono>
#include <string>

HMODULE g_module_handle;

static std::vector<std::pair<std::string, std::string>> prepare_preprocessor_definition(reshade::api::effect_runtime *runtime, reshade::api::effect_uniform_variable variable, const std::string &name)
{
    auto get_value_as_int = [runtime, variable](const char *format, int def, size_t offset = 0) {
        int value[16] = { 0 };
        runtime->get_uniform_value_int(variable, value, ARRAYSIZE(value));
        return std::format(format, value[offset]); };
    auto get_value_as_float = [runtime, variable](const char *format, float def, size_t offset = 0) {
        float value[16] = { 0 };
        runtime->get_uniform_value_float(variable, value, ARRAYSIZE(value));
        return std::format(format, value[offset]); };

    std::vector<std::pair<std::string, std::string>> prepare;
    if (name.compare("iUIUpsideDown") == 0)
        prepare.emplace_back("RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN", get_value_as_int("%d", 0));
    else if (name.compare("iUIReversed") == 0)
        prepare.emplace_back("RESHADE_DEPTH_INPUT_IS_REVERSED", get_value_as_int("%d", 0));
    else if (name.compare("iUILogarithmic") == 0)
        prepare.emplace_back("RESHADE_DEPTH_INPUT_IS_LOGARITHMIC", get_value_as_int("%d", 0));
    else if (name.compare("fUIScale") == 0)
    {
        prepare.emplace_back("RESHADE_DEPTH_INPUT_X_SCALE", get_value_as_float("%f", 0.0f, 0));
        prepare.emplace_back("RESHADE_DEPTH_INPUT_Y_SCALE", get_value_as_float("%f", 0.0f, 1));
    }
    else if (name.compare("iUIOffset") == 0)
    {
        prepare.emplace_back("RESHADE_DEPTH_INPUT_X_PIXEL_OFFSET", get_value_as_int("%d", 0, 0));
        prepare.emplace_back("RESHADE_DEPTH_INPUT_Y_PIXEL_OFFSET", get_value_as_int("%d", 0, 1));
    }
    else if (name.compare("fUIFarPlane") == 0)
        prepare.emplace_back("RESHADE_DEPTH_LINEARIZATION_FAR_PLANE", get_value_as_float("%f", 0));
    else if (name.compare("fUIDepthMultiplier") == 0)
        prepare.emplace_back("RESHADE_DEPTH_MULTIPLIER", get_value_as_float("%f", 0));

    return prepare;
}

static void on_init(reshade::api::effect_runtime *runtime)
{
    ini_file::flush_cache();

    adjust_context &ctx = runtime->create_private_data<adjust_context>();

    ctx.environment.load(runtime);
    ctx.config.load(ini_file::load_cache(ctx.environment.addon_adjustdepth_config_path));
}
static void on_destroy(reshade::api::effect_runtime *runtime)
{
    ini_file::flush_cache();

    runtime->destroy_private_data<adjust_context>();
}
static void on_reshade_present(reshade::api::effect_runtime *runtime)
{
    ini_file::flush_cache();

    adjust_context &ctx = runtime->get_private_data<adjust_context>();
    if (std::addressof(ctx) == nullptr)
        return;

    if (ctx.saving_preprocessor_definitions)
    {
        for (const auto &saving : ctx.saving_variables)
        {
            for (const auto &prepared : prepare_preprocessor_definition(runtime, saving.first, saving.second))
                runtime->set_preprocessor_definition_for_effect("GLOBAL", prepared.first.c_str(), prepared.second.c_str());
        }

        ctx.saving_variables.clear();
    }

    if (!ctx.ignore_shortcuts)
    {
        auto is_key_down = [runtime](unsigned int keycode) -> bool { return !keycode || runtime->is_key_down(keycode); };
        for (const adjustdepth_shortcut &shortcut : ctx.config.shortcuts)
        {
            const unsigned int(&keys)[4] = shortcut.overwrite_key_data;

            if (!keys[0] || !(runtime->is_key_pressed(keys[0]) && is_key_down(keys[1]) && is_key_down(keys[2]) && is_key_down(keys[3])))
                continue;

            for (std::pair<reshade::api::effect_uniform_variable, std::string> &pair : ctx.displaydepth_variables)
            {
                const reshade::api::effect_uniform_variable variable = pair.first;
                const std::string &name = pair.second;

                union uniform_value
                {
                    bool as_bool;
                    float as_float[16];
                    int32_t as_int[16];
                    uint32_t as_uint[16];
                };
                auto set_uniform_value_int = [runtime, &shortcut, variable](const std::string &definition_name, size_t offset) -> bool
                    {
                        if (auto it = shortcut.definitions.find(definition_name); it != shortcut.definitions.end())
                        {
                            uniform_value value{};
                            runtime->get_uniform_value_int(variable, value.as_int, 16, 0);
                            value.as_int[offset] = ini_data::convert<int>(it->second, 0);
                            runtime->set_uniform_value_int(variable, value.as_int, 16, 0);
                            return true;
                        }
                        else
                        {
                            return false;
                        }
                    };
                auto set_uniform_value_float = [runtime, &shortcut, variable](const std::string &definition_name, size_t offset) -> bool
                    {
                        if (auto it = shortcut.definitions.find(definition_name); it != shortcut.definitions.end())
                        {
                            uniform_value value{};
                            runtime->get_uniform_value_float(variable, value.as_float, 16, 0);
                            value.as_float[offset] = ini_data::convert<float>(it->second, 0);
                            runtime->set_uniform_value_float(variable, value.as_float, 16, 0);
                            return true;
                        }
                        else
                        {
                            return false;
                        }
                    };

                if (name.compare("iUIUpsideDown") == 0)
                    set_uniform_value_int("RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN", 0);
                else if (name.compare("iUIReversed") == 0)
                    set_uniform_value_int("RESHADE_DEPTH_INPUT_IS_REVERSED", 0);
                else if (name.compare("iUILogarithmic") == 0)
                    set_uniform_value_int("RESHADE_DEPTH_INPUT_IS_LOGARITHMIC", 0);
                else if (name.compare("fUIScale") == 0)
                {
                    set_uniform_value_float("RESHADE_DEPTH_INPUT_X_SCALE", 0);
                    set_uniform_value_float("RESHADE_DEPTH_INPUT_Y_SCALE", 1);
                }
                else if (name.compare("iUIOffset") == 0)
                {
                    set_uniform_value_int("RESHADE_DEPTH_INPUT_X_PIXEL_OFFSET", 0);
                    set_uniform_value_int("RESHADE_DEPTH_INPUT_Y_PIXEL_OFFSET", 1);
                }
                else if (name.compare("fUIFarPlane") == 0)
                    set_uniform_value_float("RESHADE_DEPTH_LINEARIZATION_FAR_PLANE", 0);
                else if (name.compare("fUIDepthMultiplier") == 0)
                    set_uniform_value_float("RESHADE_DEPTH_MULTIPLIER", 0);

                for (const auto &prepared : prepare_preprocessor_definition(runtime, variable, name))
                    runtime->set_preprocessor_definition_for_effect("GLOBAL", prepared.first.c_str(), prepared.second.c_str());
            }
        }
    }
}
static void on_reshade_reloaded_effects(reshade::api::effect_runtime *runtime)
{
    adjust_context &ctx = runtime->get_private_data<adjust_context>();
    if (std::addressof(ctx) == nullptr)
        return;

    ctx.displaydepth_variables.clear();

    runtime->enumerate_uniform_variables("DisplayDepth.fx",
        [&ctx](reshade::api::effect_runtime *runtime, reshade::api::effect_uniform_variable variable)
        {
            char name[256] = "\0";
            runtime->get_uniform_variable_name(variable, name);
            if (const auto &pair = ctx.displaydepth_variables.emplace_back(variable, name); pair.second == "bUIUseLivePreview")
            {
                ctx.ignore_preprocessor_definitions_variable = variable;
                uint32_t value = 0;
                runtime->get_uniform_value_uint(variable, &value, 1);
                ctx.ignore_preprocessor_definitions = value != 0;
            }
        });
}
static bool on_reshade_set_uniform_value(reshade::api::effect_runtime *runtime, reshade::api::effect_uniform_variable variable, const void *new_value, size_t new_value_size)
{
    adjust_context &ctx = runtime->get_private_data<adjust_context>();
    if (std::addressof(ctx) == nullptr)
        return false;

    if (auto it = std::find_if(ctx.displaydepth_variables.cbegin(), ctx.displaydepth_variables.cend(), [variable](const auto &pair) { return pair.first == variable; });
        it != ctx.displaydepth_variables.cend())
    {
        if (it->second.compare("bUIUseLivePreview") == 0)
            ctx.ignore_preprocessor_definitions = *reinterpret_cast<const unsigned int *>(new_value) != 0;
        else if (!ctx.ignore_preprocessor_definitions)
            ctx.saving_variables.emplace_back(variable, it->second);
    }

    return false;
}
static void on_reshade_overlay(reshade::api::effect_runtime *runtime)
{
    adjust_context &ctx = runtime->get_private_data<adjust_context>();
    if (std::addressof(ctx) == nullptr)
        return;

    // Disable keyboard shortcuts while typing into input boxes
    ctx.ignore_shortcuts = ImGui::IsAnyItemActive();
    ctx.saving_preprocessor_definitions = !ImGui::IsAnyItemActive();
}
static bool on_reshade_overlay_uniform_variable(reshade::api::effect_runtime *runtime, reshade::api::effect_uniform_variable variable)
{
    adjust_context &ctx = runtime->get_private_data<adjust_context>();
    if (std::addressof(ctx) == nullptr)
        return false;

    if (!ctx.displaydepth_variables.empty() && ctx.displaydepth_variables.front().first == variable)
    {
        std::string apply_button_label = ICON_FK_FLOPPY " ";
        if (ctx.ignore_preprocessor_definitions)
            apply_button_label += _("Save and complete adjustments");
        else
            apply_button_label += _("Always save adjustments is active");

        if (ImGui::Button(apply_button_label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight())))
            ctx.saving_variables = ctx.displaydepth_variables;

        ImGui::SetNextWindowSize(ImVec2(276, 160), ImGuiCond_Appearing);
        if (ImGui::BeginPopupContextItem())
        {
            const float button_size = ImGui::GetFrameHeight();
            const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;
            const float content_region_width = ImGui::GetContentRegionAvail().x;

            ImGui::SetNextItemWidth(content_region_width - button_spacing - button_size);
            reshade::imgui::key_input_box("##toggle_key", _("Save the current adjustments to the shortcut key.\nIf the shortcut key will be pressed later, adjustments will be overwritten immediately with the saved contents."), ctx.overwrite_key_data, runtime);
            ImGui::SameLine(0, button_spacing);
            ImGui::BeginDisabled(ctx.overwrite_key_data[0] == 0);
            if (ImGui::Button(ICON_FK_FLOPPY, ImVec2(button_size, 0)))
            {
                auto it = std::find_if(ctx.config.shortcuts.begin(), ctx.config.shortcuts.end(),
                     [ctx](adjustdepth_shortcut &shortcut) {
                         return std::memcmp(shortcut.overwrite_key_data, ctx.overwrite_key_data, sizeof(shortcut.overwrite_key_data)) == 0; });

                adjustdepth_shortcut &shortcut = it == ctx.config.shortcuts.end() ? ctx.config.shortcuts.emplace_back() : *it;
                if (shortcut.section.empty())
                    shortcut.section = "shortcut" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                if (shortcut.overwrite_key_data[0] == 0)
                    std::memcpy(shortcut.overwrite_key_data, ctx.overwrite_key_data, sizeof(shortcut.overwrite_key_data));

                for (const auto &saving : ctx.displaydepth_variables)
                {
                    for (const auto &prepared : prepare_preprocessor_definition(runtime, saving.first, saving.second))
                        shortcut.definitions[prepared.first] = ini_data::elements{ prepared.second };
                }

                std::memset(ctx.overwrite_key_data, 0, sizeof(ctx.overwrite_key_data));
                ctx.config.save(ini_file::load_cache(ctx.environment.addon_adjustdepth_config_path));
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                if (ImGui::BeginTooltip())
                {
                    ImGui::TextUnformatted(_("Create a shortcut to apply the current adjustments."));
                    ImGui::EndTooltip();
                }
            }
            ImGui::EndDisabled();

            for (auto it = ctx.config.shortcuts.begin(); it != ctx.config.shortcuts.end();)
            {
                ImGui::PushID(it->section.c_str(), it->section.c_str() + it->section.size());

                ImGui::BeginDisabled(true);
                ImGui::SetNextItemWidth(content_region_width - button_spacing - button_size);
                reshade::imgui::key_input_box("##toggle_key_disabled", nullptr, it->overwrite_key_data, runtime);
                ImGui::EndDisabled();
                ImGui::SameLine(0, button_spacing);
                if (reshade::imgui::confirm_button(ICON_FK_MINUS, button_size, _("Do you really want to remove the shortcut '%s'?"), key_name(it->overwrite_key_data).c_str()))
                {
                    it = ctx.config.shortcuts.erase(it);
                    ctx.config.save(ini_file::load_cache(ctx.environment.addon_adjustdepth_config_path));
                }
                else
                {
                    it++;
                }

                ImGui::PopID();
            }

            ImGui::EndPopup();
        }
    }

    return false;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        g_module_handle = hModule;

        if (!reshade::register_addon(hModule))
            return FALSE;

        reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init);
        reshade::register_event<reshade::addon_event::destroy_effect_runtime>(on_destroy);
        reshade::register_event<reshade::addon_event::reshade_present>(on_reshade_present);
        reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(on_reshade_reloaded_effects);
        reshade::register_event<reshade::addon_event::reshade_set_uniform_value>(on_reshade_set_uniform_value);
        reshade::register_event<reshade::addon_event::reshade_overlay>(on_reshade_overlay);
        reshade::register_event<reshade::addon_event::reshade_overlay_uniform_variable>(on_reshade_overlay_uniform_variable);
    }
    else if (fdwReason == DLL_PROCESS_DETACH)
    {
        reshade::unregister_addon(hModule);

        g_module_handle = nullptr;
    }

    return TRUE;
}
