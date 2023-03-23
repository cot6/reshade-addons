// SPDX-FileCopyrightText: 2018 seri14
// SPDX-License-Identifier: BSD-3-Clause

#include "dllmain.hpp"
#include "std_string_ext.hpp"

#include <reshade.hpp>

#include <string>

static bool on_reshade_set_uniform_value(reshade::api::effect_runtime *runtime, reshade::api::effect_uniform_variable variable, const void *value, size_t size)
{
    char ui_bind[256] = "";
    size_t length = ARRAYSIZE(ui_bind);

    if (!runtime->get_annotation_string_from_uniform_variable(variable, "ui_bind", ui_bind, &length))
        return false;

    ui_bind[length] = '\0';
    reshade::api::format base_type = reshade::api::format::unknown;
    uint32_t rows = 0, cols = 0, array_length = 0;
    runtime->get_uniform_variable_type(variable, &base_type, &rows, &cols, &array_length);

    if (array_length != 0)
        return false;

    union
    {
        float as_float[16];
        int32_t as_int[16];
        uint32_t as_uint[16];
    } uniform_value{};

    std::memcpy(&uniform_value, value, std::min(sizeof(uniform_value), size));

    std::string next; next.reserve(40);
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
#if 0
    switch (base_type)
    {
        case reshade::api::format::r32_typeless: // reshadefx::type::t_bool
            next = std::format("matrix<bool,%u,%u>(%*s)", rows, cols, next.size(), next.c_str());
            break;
        case reshade::api::format::r16_sint: // reshadefx::type::t_min16int:
            next = std::format("matrix<min16int,%u,%u>(%*s)", rows, cols, next.size(), next.c_str());
            break;
        case reshade::api::format::r32_sint: // reshadefx::type::t_int:
            next = std::format("matrix<int,%u,%u>(%*s)", rows, cols, next.size(), next.c_str());
            break;
        case reshade::api::format::r16_uint: // reshadefx::type::t_min16uint:
            next = std::format("matrix<min16uint,%u,%u>(%*s)", rows, cols, next.size(), next.c_str());
            break;
        case reshade::api::format::r32_uint: // reshadefx::type::t_uint:
            next = std::format("matrix<uint,%u,%u>(%*s)", rows, cols, next.size(), next.c_str());
            break;
        case reshade::api::format::r16_float: // reshadefx::type::t_min16float:
            next = std::format("matrix<min16float,%u,%u>(%*s)", rows, cols, next.size(), next.c_str());
            break;
        case reshade::api::format::r32_float: // reshadefx::type::t_float:
            next = std::format("matrix<float,%u,%u>(%*s)", rows, cols, next.size(), next.c_str());
            break;
        default: // reshade::api::format::unknown
            break;
    }
#endif

    char effect_name[256] = "";
    length = ARRAYSIZE(effect_name);
    runtime->get_uniform_variable_effect_name(variable, effect_name, &length);
    effect_name[length] = '\0';

    char prev[256] = "";
    length = ARRAYSIZE(prev);
    const bool exists = runtime->get_preprocessor_definition(ui_bind, prev, &length);
    prev[length] = '\0';

    if (!exists || next != prev)
        runtime->set_preprocessor_definition(effect_name, ui_bind, next.c_str());

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
