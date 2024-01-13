/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <imgui.h>
#include <reshade.hpp>
#include <forkawesome.h>

#include "dllmain.hpp"
#include "localization.hpp"

#include "std_string_ext.hpp"

#include <string>

HMODULE g_module_handle;

static void set_preprocessor_definition(reshade::api::effect_runtime *runtime, reshade::api::effect_uniform_variable variable, const std::string &name)
{
    auto get_value_as_int = [runtime, variable](const char *format, int def, size_t offset = 0) {
        int value[16] = { 0 };
        runtime->get_uniform_value_int(variable, value, ARRAYSIZE(value));
        return std::format(format, value[offset]); };
    auto get_value_as_float = [runtime, variable](const char *format, float def, size_t offset = 0) {
        float value[16] = { 0 };
        runtime->get_uniform_value_float(variable, value, ARRAYSIZE(value));
        return std::format(format, value[offset]); };

    if (name.compare("iUIUpsideDown") == 0)
        runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN", get_value_as_int("%d", 0).c_str());
    else if (name.compare("iUIReversed") == 0)
        runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_INPUT_IS_REVERSED", get_value_as_int("%d", 0).c_str());
    else if (name.compare("iUILogarithmic") == 0)
        runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_INPUT_IS_LOGARITHMIC", get_value_as_int("%d", 0).c_str());
    else if (name.compare("fUIScale") == 0)
    {
        runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_INPUT_X_SCALE", get_value_as_float("%f", 0.0f, 0).c_str());
        runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_INPUT_Y_SCALE", get_value_as_float("%f", 0.0f, 1).c_str());
    }
    else if (name.compare("iUIOffset") == 0)
    {
        runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_INPUT_X_PIXEL_OFFSET", get_value_as_int("%d", 0, 0).c_str());
        runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_INPUT_Y_PIXEL_OFFSET", get_value_as_int("%d", 0, 1).c_str());
    }
    else if (name.compare("fUIFarPlane") == 0)
        runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_LINEARIZATION_FAR_PLANE", get_value_as_float("%f", 0).c_str());
    else if (name.compare("fUIDepthMultiplier") == 0)
        runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_MULTIPLIER", get_value_as_float("%f", 0).c_str());
}

static void on_init(reshade::api::effect_runtime *runtime)
{
    runtime->create_private_data<adjust_context>();
}
static void on_destroy(reshade::api::effect_runtime *runtime)
{
    runtime->destroy_private_data<adjust_context>();
}
static void on_reshade_present(reshade::api::effect_runtime *runtime)
{
    adjust_context &ctx = runtime->get_private_data<adjust_context>();

    if (ctx.saving_preprocessor_definitions)
    {
        for (const auto &pair : ctx.saving_variables)
            set_preprocessor_definition(runtime, pair.first, pair.second);

        ctx.saving_variables.clear();
    }
}
static void on_reshade_reloaded_effects(reshade::api::effect_runtime *runtime)
{
    adjust_context &ctx = runtime->get_private_data<adjust_context>();
    ctx.displaydepth_variables.clear();

    runtime->enumerate_uniform_variables("DisplayDepth.fx",
        [runtime, &ctx](reshade::api::effect_runtime *, reshade::api::effect_uniform_variable variable)
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

    ctx.saving_preprocessor_definitions = !ImGui::IsAnyItemActive();
}
static bool on_reshade_overlay_uniform_variable(reshade::api::effect_runtime *runtime, reshade::api::effect_uniform_variable variable)
{
    adjust_context &ctx = runtime->get_private_data<adjust_context>();

    if (!ctx.displaydepth_variables.empty() && ctx.displaydepth_variables.front().first == variable)
    {
        ImGui::BeginDisabled(!ctx.ignore_preprocessor_definitions);

        std::string apply_button_label = ICON_FK_FLOPPY " ";
        if (ctx.ignore_preprocessor_definitions)
            apply_button_label += _("Save and complete adjustments");
        else
            apply_button_label += _("Always save adjustments is active");

        if (ImGui::Button(apply_button_label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight())))
            ctx.saving_variables = ctx.displaydepth_variables;

        ImGui::EndDisabled();
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
