/*
 * SPDX-FileCopyrightText: Copyright (C) 2014 Patrick Mours
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "input.hpp"
#include "imgui_widgets.hpp"
#include "localization.hpp"

#include <forkawesome.h>
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
    else if (hint && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        if (ImGui::BeginTooltip())
        {
            std::string tooltip = hint;
            tooltip.append(2, '\n');
            tooltip += _("Click in the field and press any key to change the shortcut to that key or press backspace to remove the shortcut.");
            ImGui::TextUnformatted(tooltip.c_str(), tooltip.c_str() + tooltip.size());
            ImGui::EndTooltip();
        }
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

bool reshade::imgui::popup_button(const char *label, float width, ImGuiWindowFlags flags)
{
    if (ImGui::Button(label, ImVec2(width, 0)))
        ImGui::OpenPopup(label); // Popups can have the same ID as other items without conflict
    return ImGui::BeginPopup(label, flags);
}

bool reshade::imgui::confirm_button(const char *label, float width, const char *message, ...)
{
    bool res = false;

    if (popup_button(label, width))
    {
        va_list args;
        va_start(args, message);
        ImGui::TextV(message, args);
        va_end(args);

        const float button_width = (ImGui::GetContentRegionAvail().x / 2) - ImGui::GetStyle().ItemInnerSpacing.x;

        std::string button_label;
        button_label = ICON_FK_OK " ";
        button_label += _("Yes");

        if (ImGui::Button(button_label.c_str(), ImVec2(button_width, 0)))
        {
            ImGui::CloseCurrentPopup();
            res = true;
        }

        ImGui::SameLine();

        button_label = ICON_FK_CANCEL " ";
        button_label += _("No");

        if (ImGui::Button(button_label.c_str(), ImVec2(button_width, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return res;
}
