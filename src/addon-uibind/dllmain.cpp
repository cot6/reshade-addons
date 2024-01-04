// SPDX-FileCopyrightText: 2018 seri14
// SPDX-License-Identifier: BSD-3-Clause

#include "dllmain.hpp"
#include "std_string_ext.hpp"

#include <reshade.hpp>

#include <string>

static bool on_reshade_set_uniform_value(reshade::api::effect_runtime *runtime, reshade::api::effect_uniform_variable variable, const void *value, size_t size)
{
    char buf[260] = "";
    size_t len = sizeof(buf);

    if (!runtime->get_annotation_string_from_uniform_variable(variable, "ui_bind", buf, &len))
        return false;
    const std::string ui_bind(buf, len);

    reshade::api::format base_type = reshade::api::format::unknown;
    uint32_t rows = 0, cols = 0, array_length = 0, components = 0;
    runtime->get_uniform_variable_type(variable, &base_type, &rows, &cols, &array_length);
    components = rows * cols;

    if (array_length != 0)
        return false;

    union
    {
        float as_float[16];
        int32_t as_int[16];
        uint32_t as_uint[16];
    } def{}, uniform_value{};

    std::memcpy(&uniform_value, value, std::min(sizeof(uniform_value), size));

    bool is_default_value = false;
    switch (base_type)
    {
        case reshade::api::format::r32_typeless:
            runtime->reset_uniform_value(variable);
            runtime->get_uniform_value_uint(variable, def.as_uint, components);
            for (size_t i = 0; !is_default_value && i < components; i++)
                is_default_value = (uniform_value.as_uint[i] != 0) == (def.as_uint[i] != 0);
            runtime->set_uniform_value_uint(variable, uniform_value.as_uint, components);
            break;
        case reshade::api::format::r32_uint:
        case reshade::api::format::r32_sint:
        case reshade::api::format::r16_uint:
        case reshade::api::format::r16_sint:
            runtime->reset_uniform_value(variable);
            runtime->get_uniform_value_uint(variable, def.as_uint, components);
            is_default_value = std::memcmp(uniform_value.as_uint, def.as_uint, components * sizeof(int)) == 0;
            runtime->set_uniform_value_uint(variable, uniform_value.as_uint, components);
            break;
        case reshade::api::format::r32_float:
        case reshade::api::format::r16_float:
            float threshold = 7.5e-4f + FLT_EPSILON;
            if (runtime->get_annotation_float_from_uniform_variable(variable, "ui_step", &threshold, 1))
                threshold = threshold * 0.75f + FLT_EPSILON;
            runtime->reset_uniform_value(variable);
            runtime->get_uniform_value_float(variable, def.as_float, components);
            for (size_t i = 0; !is_default_value && i < components; i++)
                is_default_value = std::abs(uniform_value.as_float[i] - def.as_float[i]) < threshold;
            runtime->set_uniform_value_float(variable, uniform_value.as_float, components);
            break;
    }

    std::string next;

    if (!is_default_value)
    {
        next.reserve(40);
        for (size_t row = 0; row < rows; ++row)
        {
            for (size_t col = 0; col < cols; ++col)
            {
                const size_t buffer_index = cols * row + col;
                switch (base_type)
                {
                    case reshade::api::format::r32_typeless: // reshadefx::type::t_bool
                        next += uniform_value.as_uint[buffer_index] ? "1" : "0";
                        break;
                    case reshade::api::format::r16_sint: // reshadefx::type::t_min16int:
                    case reshade::api::format::r32_sint: // reshadefx::type::t_int:
                        next += std::format("%d", uniform_value.as_int[buffer_index]);
                        break;
                    case reshade::api::format::r16_uint: // reshadefx::type::t_min16uint:
                    case reshade::api::format::r32_uint: // reshadefx::type::t_uint:
                        next += std::format("%u", uniform_value.as_uint[buffer_index]);
                        break;
                    case reshade::api::format::r16_float: // reshadefx::type::t_min16float:
                    case reshade::api::format::r32_float: // reshadefx::type::t_float:
                        next += std::format("%.8e", uniform_value.as_float[buffer_index]);
                        break;
                    default: // reshade::api::format::unknown
                        next += '0';
                        break;
                }
                next += ',';
            }
        }
        if (!next.empty())
            next.resize(next.size() - 1);
    }

    len = sizeof(buf);
    runtime->get_uniform_variable_effect_name(variable, buf, &len);
    const std::string effect_name(buf, len);

    len = sizeof(buf);
    const bool exists = runtime->get_preprocessor_definition(ui_bind.c_str(), buf, &len);
    const std::string prev(buf, len);

    if (!exists || next.empty() || next != prev)
    {
        const char *definition = next.empty() ? nullptr : next.c_str();
        runtime->set_preprocessor_definition_for_effect(effect_name.c_str(), ui_bind.c_str(), definition);
    }

    return false;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            if (!reshade::register_addon(hModule))
                return FALSE;
            reshade::register_event<reshade::addon_event::reshade_set_uniform_value>(on_reshade_set_uniform_value);
            break;
        case DLL_PROCESS_DETACH:
            reshade::unregister_addon(hModule);
            break;
    }

    return TRUE;
}
