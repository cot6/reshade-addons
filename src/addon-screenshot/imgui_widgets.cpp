/*
 * SPDX-FileCopyrightText: Copyright (C) 2014 Patrick Mours
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "input.hpp"
#include "imgui_widgets.hpp"
#include "localization.hpp"

#include <imgui.h>
#include <reshade.hpp>

bool reshade::imgui::key_input_box(const char *name, const char *hint, unsigned int(&key)[4], reshade::api::effect_runtime *runtime)
{
    bool res = false;
    char buf[48]{};
    if (key[0] || key[1] || key[2] || key[3])
        buf[key_name(key).copy(buf, sizeof(buf) - 1)] = '\0';

    ImGui::InputTextWithHint(name, _("Click to set keyboard shortcut"), buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_NoHorizontalScroll);

    if (ImGui::IsItemActive())
    {
        if (const unsigned int last_key_pressed = runtime->last_key_pressed(); last_key_pressed != 0)
        {
            if (last_key_pressed == 0x08) // Backspace
            {
                key[0] = 0;
                key[1] = 0;
                key[2] = 0;
                key[3] = 0;
            }
            else if (last_key_pressed != 0x10 && last_key_pressed != 0x11 && last_key_pressed != 0x12) // Exclude modifier keys
            {
                key[0] = last_key_pressed;
                key[1] = runtime->is_key_down(0x11) ? 0x11 : 0; // Ctrl
                key[2] = runtime->is_key_down(0x10) ? 0x10 : 0; // Shift
                key[3] = runtime->is_key_down(0x12) ? 0x12 : 0; // Alt

                res = true;
            }
        }
    }
    else if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        std::string tooltip = hint;
        tooltip += "\n\n";
        tooltip += _("Click in the field and press any key to change the shortcut to that key or press backspace to remove the shortcut.");
        ImGui::SetTooltip("%*s", tooltip.size(), tooltip.c_str());
    }

    return res;
}

bool reshade::imgui::radio_list(const char *label, const std::string_view ui_items, int &v)
{
    ImGui::BeginGroup();

    bool res = false;

    const float item_width = ImGui::CalcItemWidth();

    // Group all radio buttons together into a list
    ImGui::BeginGroup();

    for (size_t offset = 0, next, i = 0; (next = ui_items.find('\0', offset)) != std::string_view::npos; offset = next + 1, ++i)
        res |= ImGui::RadioButton(ui_items.data() + offset, &v, static_cast<int>(i));

    ImGui::EndGroup();

    ImGui::SameLine(item_width, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::TextUnformatted(label);

    ImGui::EndGroup();

    return res;
}
