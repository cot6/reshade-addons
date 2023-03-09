﻿// SPDX-FileCopyrightText: 2018 seri14
// SPDX-License-Identifier: BSD-3-Clause

#include "dllmain.hpp"

#include <algorithm>
#include <list>
#include <string>
#include <vector>

constexpr size_t HISTORY_LIMIT = 1000;

struct history
{
    enum class kind
    {
        uniform_value = 0,
        technique_state,
        technique_sort,
    };

    union uniform_value
    {
        bool as_bool;
        float as_float[16];
        int32_t as_int[16];
        uint32_t as_uint[16];
    };

    kind kind = kind::uniform_value;

    reshade::api::effect_technique technique_handle = { 0 };
    std::string technique_name;
    bool technique_enabled = false;

    reshade::api::effect_uniform_variable variable_handle = { 0 };
    reshade::api::format variable_basetype = reshade::api::format::unknown;
    uniform_value before = {}, after = {};
    std::vector<reshade::api::effect_technique> sorted, sorting;
    bool confirmed = false;
};

struct __declspec(uuid("2f91f8ec-6f8e-436b-b6cc-d7f8d5f9e44c")) history_context
{
    bool was_updated = false;
    size_t history_pos = 0;
    std::list<history> histories;
};

static bool on_reorder_techniques(reshade::api::effect_runtime *runtime, size_t count, reshade::api::effect_technique *sorting_techniques)
{
    history_context &ctx = runtime->get_private_data<history_context>();

    std::vector<reshade::api::effect_technique> sorted, sorting;
    runtime->enumerate_techniques(nullptr,
        [&sorted](reshade::api::effect_runtime *, reshade::api::effect_technique &technique) {
            sorted.push_back(technique);
        });

    sorting.resize(sorted.size());
    std::memcpy(sorting.data(), sorting_techniques, sizeof(reshade::api::effect_technique) * count);

    while (ctx.history_pos > 0)
    {
        ctx.histories.pop_front();
        --ctx.history_pos;
    }

    if (auto front = ctx.histories.begin(); front != ctx.histories.end() && front->kind == history::kind::technique_sort)
    {
        sorted = std::move(front->sorted);
        ctx.histories.pop_front();
    }

    history history;
    history.kind = history::kind::technique_sort;
    history.sorted = std::move(sorted);
    history.sorting = std::move(sorting);

    if (ctx.histories.size() < HISTORY_LIMIT)
        ctx.histories.push_front(std::move(history));

    return false;
}

static void on_init(reshade::api::effect_runtime *runtime)
{
    runtime->create_private_data<history_context>();
}
static void on_destroy(reshade::api::effect_runtime *runtime)
{
    runtime->destroy_private_data<history_context>();
}

static void on_reshade_set_current_preset_path(reshade::api::effect_runtime *runtime, const char *)
{
    history_context &ctx = runtime->get_private_data<history_context>();

    ctx.histories.clear();
    ctx.history_pos = 0;
    ctx.was_updated = false;
}

static bool on_set_uniform_value(reshade::api::effect_runtime *runtime, reshade::api::effect_uniform_variable variable, const void *value, size_t size)
{
    history_context &ctx = runtime->get_private_data<history_context>();

    char ui_type[16] = "";
    if (!runtime->get_annotation_string_from_uniform_variable(variable, "ui_type", ui_type))
        return false;

    reshade::api::format basetype;
    runtime->get_uniform_variable_type(variable, &basetype);

    history::uniform_value before = {}, after = {};

    switch (basetype)
    {
        case reshade::api::format::r32_typeless:
            runtime->get_uniform_value_bool(variable, &before.as_bool, 1);
            break;
        case reshade::api::format::r32_float:
            runtime->get_uniform_value_float(variable, before.as_float, 16);
            break;
        case reshade::api::format::r32_sint:
            runtime->get_uniform_value_int(variable, before.as_int, 16);
            break;
        case reshade::api::format::r32_uint:
            runtime->get_uniform_value_uint(variable, before.as_uint, 16);
            break;
    }

    std::memcpy(after.as_uint, value, std::min(size, size_t(4 * 16)));

    if (std::memcmp(&before, &after, sizeof(after)) != 0)
    {
        history history;
        history.kind = history::kind::uniform_value;
        history.variable_handle = variable;
        history.variable_basetype = basetype;
        history.before = before;
        history.after = after;

        while (ctx.history_pos > 0)
        {
            ctx.histories.pop_front();
            --ctx.history_pos;
        }

        if (auto front = ctx.histories.begin(); front != ctx.histories.end() && front->variable_handle.handle == variable.handle)
        {
            std::memcpy(&history.before, &front->before, sizeof(history.before));
            ctx.histories.pop_front();
        }

        if (ctx.histories.size() < HISTORY_LIMIT)
            ctx.histories.push_front(std::move(history));

        ctx.history_pos = 0;
        ctx.was_updated = true;
    }

    return false;
}
static bool on_set_technique_state(reshade::api::effect_runtime *runtime, reshade::api::effect_technique technique, bool enabled)
{
    history_context &ctx = runtime->get_private_data<history_context>();

    char technique_name[128] = "";
    runtime->get_technique_name(technique, technique_name);

    if (const auto it = std::next(ctx.histories.begin(), ctx.history_pos);
        it == ctx.histories.end() ||
        it->kind != history::kind::technique_state ||
        it->technique_name != technique_name ||
        enabled == it->technique_enabled)
    {
        history history;
        history.kind = history::kind::technique_state;
        history.technique_handle = technique;
        history.technique_name = technique_name;
        history.technique_enabled = enabled;

        while (ctx.history_pos > 0)
        {
            ctx.histories.pop_front();
            --ctx.history_pos;
        }

        if (ctx.histories.size() < HISTORY_LIMIT)
            ctx.histories.push_front(std::move(history));
    }
    else
    {
        ctx.history_pos++;
    }

    return false;
}

static void draw_history_window(reshade::api::effect_runtime *runtime)
{
    size_t current_pos = std::numeric_limits<size_t>::max();
    size_t selected_pos = std::numeric_limits<size_t>::max();

    history_context &ctx = runtime->get_private_data<history_context>();

    if (ImGui::Selectable("End of Undo", ctx.history_pos == ctx.histories.size()))
        selected_pos = ctx.histories.size();

    if (ctx.histories.empty())
        return;

    current_pos = ctx.histories.size() - 1;
    bool modified = false;

    for (auto it = ctx.histories.rbegin(); it != ctx.histories.rend(); --current_pos, ++it)
    {
        std::string label;

        switch (it->kind)
        {
            case history::kind::uniform_value:
            {
                char ui_type[16] = "";
                runtime->get_annotation_string_from_uniform_variable(it->variable_handle, "ui_type", ui_type);

                char variable_name[128] = "";
                runtime->get_uniform_variable_name(it->variable_handle, variable_name);

                label += variable_name;

                reshade::api::format basetype; unsigned int rows;
                runtime->get_uniform_variable_type(it->variable_handle, &basetype, &rows);

                for (unsigned int i = 0; i < rows; ++i)
                {
                    char value[80] = "";

                    if (basetype == reshade::api::format::r32_typeless)
                    {
                        if (strcmp(ui_type, "combo") == 0)
                        {
                            label += it->after.as_bool ? " On" : " Off";
                        }
                        else
                        {
                            label += it->after.as_bool ? " True" : " False";
                        }
                    }
                    else if (basetype == reshade::api::format::r32_float)
                    {
                        if (strcmp(ui_type, "color") == 0)
                        {
                            sprintf_s(value, " %c %+0.0f (%0.0f)", "RGBA"[i], (it->after.as_float[i] - it->before.as_float[i]) / (1.0f / 255.0f), it->after.as_float[i] / (1.0f / 255.0f));
                        }
                        else
                        {
                            float ui_stp_val = 0.0f;
                            runtime->get_annotation_float_from_uniform_variable(it->variable_handle, "ui_step", &ui_stp_val, 1);
                            if (FLT_EPSILON > ui_stp_val)
                                ui_stp_val = 0.001f;

                            // Calculate display precision based on step value
                            int precision = 0;
                            for (float x = 1.0f; x * ui_stp_val < 1.0f && precision < 9; x *= 10.0f)
                                ++precision;

                            sprintf_s(value, " %c %+0.*f (%0.*f)", "XYZW"[i], precision, it->after.as_float[i] - it->before.as_float[i], precision, it->after.as_float[i]);
                        }
                    }
                    else if (basetype == reshade::api::format::r32_sint || basetype == reshade::api::format::r32_uint)
                    {
                        char ui_items[512] = ""; size_t ui_items_len = sizeof(ui_items);
                        runtime->get_annotation_string_from_uniform_variable(it->variable_handle, "ui_items", ui_items, &ui_items_len);

                        if (strcmp(ui_type, "combo") == 0)
                        {
                            size_t ui_items_offset = 0;
                            for (uint32_t ui_items_index = 0; ui_items_offset < ui_items_len && ui_items_index != it->after.as_uint[0]; ++ui_items_offset)
                                if (ui_items[ui_items_offset] == '\0')
                                    ++ui_items_index;

                            sprintf_s(value, " %+lld (%s)", static_cast<int64_t>(it->after.as_uint[i]) - static_cast<int64_t>(it->before.as_uint[i]), ui_items + ui_items_offset);
                        }
                        else if (basetype == reshade::api::format::r32_sint)
                        {
                            sprintf_s(value, " %c %+lld (%d)", (strcmp(ui_type, "color") == 0 ? "RGBA" : "XYZW")[i], static_cast<int64_t>(it->after.as_int[i]) - static_cast<int64_t>(it->before.as_int[i]), it->after.as_int[i]);
                        }
                        else
                        {
                            sprintf_s(value, " %c %+lld (%u)", (strcmp(ui_type, "color") == 0 ? "RGBA" : "XYZW")[i], static_cast<int64_t>(it->after.as_uint[i]) - static_cast<int64_t>(it->before.as_uint[i]), it->after.as_uint[i]);
                        }
                    }

                    label += value;
                }
                break;
            }
            case history::kind::technique_state:
            {
                label += it->technique_name;
                label += it->technique_enabled ? " True" : " False";
                break;
            }
            case history::kind::technique_sort:
            {
                label += "Sort technique list";
                break;
            }
        }

        label += "##" + std::to_string(current_pos);

        if (ImGui::Selectable(label.c_str(), current_pos == ctx.history_pos))
        {
            modified = true;
            selected_pos = current_pos;
        }

        if (ctx.was_updated && current_pos == ctx.history_pos)
        {
            ctx.was_updated = false;
            ImGui::SetScrollHereY();
        }
    }

    if (selected_pos == ctx.history_pos || selected_pos == std::numeric_limits<size_t>::max())
        return;

    auto it = std::next(ctx.histories.begin(), ctx.history_pos);
    auto distance = static_cast<ptrdiff_t>(selected_pos) - static_cast<ptrdiff_t>(ctx.history_pos);

    ctx.history_pos = selected_pos;

    if (distance > 0)
    {
        while (distance-- > 0)
        {
            switch (it->kind)
            {
                case history::kind::uniform_value:
                    switch (it->variable_basetype)
                    {
                        case reshade::api::format::r32_typeless:
                            runtime->set_uniform_value_bool(it->variable_handle, &it->before.as_bool, 1);
                            break;
                        case reshade::api::format::r32_float:
                            runtime->set_uniform_value_float(it->variable_handle, it->before.as_float, 16);
                            break;
                        case reshade::api::format::r32_sint:
                            runtime->set_uniform_value_int(it->variable_handle, it->before.as_int, 16);
                            break;
                        case reshade::api::format::r32_uint:
                            runtime->set_uniform_value_uint(it->variable_handle, it->before.as_uint, 16);
                            break;
                    }
                    break;
                case history::kind::technique_state:
                    runtime->set_technique_state(it->technique_handle, !it->technique_enabled);
                    break;
                case history::kind::technique_sort:
                    runtime->reorder_techniques(it->sorted.size(), it->sorted.data());
                    break;
            }

            ++it;
        }
    }
    else
    {
        while (distance++ < 0)
        {
            --it;

            switch (it->kind)
            {
                case history::kind::uniform_value:
                    switch (it->variable_basetype)
                    {
                        case reshade::api::format::r32_typeless:
                            runtime->set_uniform_value_bool(it->variable_handle, &it->after.as_bool, 1);
                            break;
                        case reshade::api::format::r32_float:
                            runtime->set_uniform_value_float(it->variable_handle, it->after.as_float, 16);
                            break;
                        case reshade::api::format::r32_sint:
                            runtime->set_uniform_value_int(it->variable_handle, it->after.as_int, 16);
                            break;
                        case reshade::api::format::r32_uint:
                            runtime->set_uniform_value_uint(it->variable_handle, it->after.as_uint, 16);
                            break;
                    }
                    break;
                case history::kind::technique_state:
                    runtime->set_technique_state(it->technique_handle, it->technique_enabled);
                    break;
                case history::kind::technique_sort:
                    runtime->reorder_techniques(it->sorting.size(), it->sorting.data());
                    break;
            }
        }
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            if (!reshade::register_addon(hModule))
                return FALSE;
            reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init);
            reshade::register_event<reshade::addon_event::destroy_effect_runtime>(on_destroy);
            reshade::register_event<reshade::addon_event::reshade_set_current_preset_path>(on_reshade_set_current_preset_path);
            reshade::register_event<reshade::addon_event::reshade_set_uniform_value>(on_set_uniform_value);
            reshade::register_event<reshade::addon_event::reshade_set_technique_state>(on_set_technique_state);
            reshade::register_event<reshade::addon_event::reshade_reorder_techniques>(on_reorder_techniques);
            reshade::register_overlay("History", draw_history_window);
            break;
        case DLL_PROCESS_DETACH:
            reshade::unregister_overlay("History", draw_history_window);
            reshade::unregister_addon(hModule);
            break;
    }

    return TRUE;
}
