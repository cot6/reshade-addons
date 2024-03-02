// SPDX-FileCopyrightText: 2018 seri14
// SPDX-License-Identifier: BSD-3-Clause

#include "dllmain.hpp"
#include "localization.hpp"
#include "std_string_ext.hpp"

#include <imgui.h>
#include <reshade.hpp>

#include <string>
#include <vector>

HMODULE g_module_handle;

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
static void on_reshade_reloaded_effects(reshade::api::effect_runtime *runtime)
{
    history_context &ctx = runtime->get_private_data<history_context>();
    size_t techniques_count = 0;

    runtime->enumerate_techniques(nullptr, [&techniques_count](reshade::api::effect_runtime *, reshade::api::effect_technique) { techniques_count++; });

    if (ctx.techniques_count != techniques_count)
        ctx.techniques_count = techniques_count;

    if (ctx.techniques_count == 0)
        ctx.histories.clear();
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
    // Ignore special uniform variables
    if (runtime->get_annotation_string_from_uniform_variable(variable, "source", nullptr, nullptr))
        return false;

    history_context &ctx = runtime->get_private_data<history_context>();
    ctx.was_updated = true;

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
        default:
            return false; // Unknown type from future version
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
            ctx.history_pos--;
        }

        if (auto front = ctx.histories.begin(); front != ctx.histories.end() && front->variable_handle.handle == variable.handle)
        {
            std::memcpy(&history.before, &front->before, sizeof(history.before));
            ctx.histories.pop_front();
        }

        if (ctx.histories.size() < HISTORY_LIMIT)
            ctx.histories.push_front(std::move(history));

        ctx.history_pos = 0;
    }

    return false;
}
static bool on_set_technique_state(reshade::api::effect_runtime *runtime, reshade::api::effect_technique technique, bool enabled)
{
    if (runtime->get_annotation_int_from_technique(technique, "hidden", nullptr, 0) ||
        runtime->get_annotation_int_from_technique(technique, "enabled", nullptr, 0) ||
        runtime->get_annotation_int_from_technique(technique, "enabled_in_screenshot", nullptr, 0) ||
        runtime->get_annotation_int_from_technique(technique, "timeout", nullptr, 0))
        return false;

    history_context &ctx = runtime->get_private_data<history_context>();
    ctx.was_updated = true;

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
            ctx.history_pos--;
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

    bool selected = ctx.history_pos == ctx.histories.size();
    if (ImGui::Selectable(_("End of Undo"), selected))
        selected_pos = ctx.histories.size();

    if (ctx.was_updated && selected)
    {
        ctx.was_updated = false;
        ImGui::SetScrollHereY();
    }

    if (!ctx.histories.empty())
    {
        current_pos = ctx.histories.size() - 1;

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

                    reshade::api::format basetype; unsigned int rows;
                    runtime->get_uniform_variable_type(it->variable_handle, &basetype, &rows);

                    label += _("Variable");
                    label += ' ';
                    label += variable_name;
                    for (unsigned int i = 0; i < rows; ++i)
                    {
                        label += ' ';
                        if (rows > 1)
                        {
                            label += (strcmp(ui_type, "color") == 0 ? "RGBA" : "XYZW")[i];
                            label += ' ';
                        }
                        if (basetype == reshade::api::format::r32_typeless)
                        {
                            label += it->after.as_bool ? "True" : "False";
                        }
                        else if (basetype == reshade::api::format::r32_float)
                        {
                            if (strcmp(ui_type, "color") == 0)
                            {
                                label += std::format("%+0.0f (%0.0f)", (it->after.as_float[i] - it->before.as_float[i]) / (1.0f / 255.0f), it->after.as_float[i] / (1.0f / 255.0f));
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

                                label += std::format("%+0.*f (%0.*f)", precision, it->after.as_float[i] - it->before.as_float[i], precision, it->after.as_float[i]);
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

                                label += std::format("%+lld (%s)", static_cast<int64_t>(it->after.as_uint[i]) - static_cast<int64_t>(it->before.as_uint[i]), ui_items + ui_items_offset);
                            }
                            else if (basetype == reshade::api::format::r32_sint)
                            {
                                label += std::format("%+lld (%d)", static_cast<int64_t>(it->after.as_int[i]) - static_cast<int64_t>(it->before.as_int[i]), it->after.as_int[i]);
                            }
                            else
                            {
                                label += std::format("%+lld (%u)", static_cast<int64_t>(it->after.as_uint[i]) - static_cast<int64_t>(it->before.as_uint[i]), it->after.as_uint[i]);
                            }
                        }
                    }
                    break;
                }
                case history::kind::technique_state:
                {
                    label += _("Technique");
                    label += ' ';
                    label += it->technique_name;
                    label += ' ';
                    label += it->technique_enabled ? _("Enable") : _("Disable");
                    break;
                }
                case history::kind::technique_sort:
                {
                    label += _("Sort technique list");
                    break;
                }
            }

            label += "##" + std::to_string(current_pos);

            selected = current_pos == ctx.history_pos;
            if (ImGui::Selectable(label.c_str(), selected))
                selected_pos = current_pos;

            if (ctx.was_updated && selected)
            {
                ctx.was_updated = false;
                ImGui::SetScrollHereY();
            }
        }
    }

    if (ctx.was_updated)
        ctx.was_updated = false;

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

static bool on_reshade_open_overlay(reshade::api::effect_runtime *runtime, bool open, reshade::api::input_source)
{
    history_context &ctx = runtime->get_private_data<history_context>();
    ctx.show_overlay = open;

    return false;
}
static void on_reshade_overlay(reshade::api::effect_runtime *runtime)
{
    history_context &ctx = runtime->get_private_data<history_context>();

    if (!ctx.show_overlay)
        return;

    ImGui::SetNextWindowSize(ImVec2(320, 240), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 320 - ImGui::GetStyle().WindowPadding.x, ImGui::GetStyle().WindowPadding.y + 104), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(_("History###editorhistory"), nullptr, ImGuiWindowFlags_NoFocusOnAppearing))
        draw_history_window(runtime);
    ImGui::End();
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
        reshade::register_event<reshade::addon_event::reshade_set_current_preset_path>(on_reshade_set_current_preset_path);
        reshade::register_event<reshade::addon_event::reshade_set_uniform_value>(on_set_uniform_value);
        reshade::register_event<reshade::addon_event::reshade_set_technique_state>(on_set_technique_state);
        reshade::register_event<reshade::addon_event::reshade_reorder_techniques>(on_reorder_techniques);
        reshade::register_event<reshade::addon_event::reshade_overlay>(on_reshade_overlay);
        reshade::register_event<reshade::addon_event::reshade_open_overlay>(on_reshade_open_overlay);
    }
    else if (fdwReason == DLL_PROCESS_DETACH)
    {
        reshade::unregister_addon(hModule);

        g_module_handle = nullptr;
    }

    return TRUE;
}
