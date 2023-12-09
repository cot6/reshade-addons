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

static void on_init(reshade::api::effect_runtime *runtime)
{
    runtime->create_private_data<adjust_context>();
}
static void on_destroy(reshade::api::effect_runtime *runtime)
{
    runtime->destroy_private_data<adjust_context>();
}
static void on_reshade_reloaded_effects(reshade::api::effect_runtime *runtime)
{
    adjust_context &ctx = runtime->get_private_data<adjust_context>();
    ctx.variables.clear();

    runtime->enumerate_uniform_variables("DisplayDepth.fx",
        [&ctx](reshade::api::effect_runtime *runtime, reshade::api::effect_uniform_variable variable)
        {
            char name[256]; *name = '\0';
            runtime->get_uniform_variable_name(variable, name);
            ctx.variables.emplace_back(variable, name);
        });
}
static bool on_reshade_overlay_uniform_variable(reshade::api::effect_runtime *runtime, reshade::api::effect_uniform_variable variable)
{
    adjust_context &ctx = runtime->get_private_data<adjust_context>();

    if (!ctx.variables.empty() && ctx.variables.front().first == variable)
    {
        std::string apply_button_label = ICON_FK_FLOPPY " ";
        apply_button_label += _("Save and complete adjustments");
        if (ImGui::Button(apply_button_label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight())))
        {
            for (const std::pair<reshade::api::effect_uniform_variable, std::string> &pair : ctx.variables)
            {
                const reshade::api::effect_uniform_variable &uniform = pair.first;
                const std::string &name = pair.second;

                auto get_value_as_int = [runtime, uniform](const char *format, int def, size_t offset = 0) {
                    int value[16] = { 0 };
                    runtime->get_uniform_value_int(uniform, value, ARRAYSIZE(value));
                    return std::format(format, value[offset]); };
                auto get_value_as_float = [runtime, uniform](const char *format, float def, size_t offset = 0) {
                    float value[16] = { 0 };
                    runtime->get_uniform_value_float(uniform, value, ARRAYSIZE(value));
                    return std::format(format, value[offset]); };

                if (_stricmp(name.c_str(), "iUIUpsideDown") == 0)
                    runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN", get_value_as_int("%d", 0).c_str());
                else if (_stricmp(name.c_str(), "iUIReversed") == 0)
                    runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_INPUT_IS_REVERSED", get_value_as_int("%d", 0).c_str());
                else if (_stricmp(name.c_str(), "iUILogarithmic") == 0)
                    runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_INPUT_IS_LOGARITHMIC", get_value_as_int("%d", 0).c_str());
                else if (_stricmp(name.c_str(), "fUIScale") == 0)
                {
                    runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_INPUT_X_SCALE", get_value_as_float("%f", 0.0f, 0).c_str());
                    runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_INPUT_Y_SCALE", get_value_as_float("%f", 0.0f, 1).c_str());
                }
                else if (_stricmp(name.c_str(), "iUIOffset") == 0)
                {
                    runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_INPUT_X_PIXEL_OFFSET", get_value_as_int("%d", 0, 0).c_str());
                    runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_INPUT_Y_PIXEL_OFFSET", get_value_as_int("%d", 0, 1).c_str());
                }
                else if (_stricmp(name.c_str(), "fUIFarPlane") == 0)
                    runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_LINEARIZATION_FAR_PLANE", get_value_as_float("%f", 0).c_str());
                else if (_stricmp(name.c_str(), "fUIDepthMultiplier") == 0)
                    runtime->set_preprocessor_definition_for_effect("GLOBAL", "RESHADE_DEPTH_MULTIPLIER", get_value_as_float("%f", 0).c_str());
            }
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
        reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(on_reshade_reloaded_effects);
        reshade::register_event<reshade::addon_event::reshade_overlay_uniform_variable>(on_reshade_overlay_uniform_variable);
    }
    else if (fdwReason == DLL_PROCESS_DETACH)
    {
        reshade::unregister_addon(hModule);

        g_module_handle = nullptr;
    }

    return TRUE;
}
