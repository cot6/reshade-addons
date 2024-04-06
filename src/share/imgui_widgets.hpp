/*
 * SPDX-FileCopyrightText: Copyright (C) 2022 Patrick Mours
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <imgui.h>
#include <reshade.hpp>

namespace reshade::imgui
{
	/// <summary>
	/// Adds a keyboard shortcut widget.
	/// </summary>
	/// <param name="label">Label text describing this widget.</param>
	/// <param name="key">Shortcut, consisting of the [virtual key code, Ctrl, Shift, Alt].</param>
	bool key_input_box(const char *name, const char *hint, unsigned int(&key)[4], reshade::api::effect_runtime *runtime);

	/// <summary>
	/// Adds a widget which shows a vertical list of radio buttons plus a label to the right.
	/// </summary>
	/// <param name="label">Label text describing this widget.</param>
	/// <param name="ui_items">List of labels for the items, separated with '\0' characters.</param>
	/// <param name="v">Index of the active item in the <paramref name="ui_items"/> list.</param>
	bool radio_list(const char *label, const std::string_view ui_items, int &v);

    /// <summary>
    /// Convenience function which adds a button that when pressed opens a popup window.
    /// Begins the popup window and returns <see langword="true"/> if it is open. Don't forget to call 'ImGui::EndPopup'.
    /// </summary>
    bool popup_button(const char *label, float width = 0.0f, ImGuiWindowFlags flags = 0);

    /// <summary>
    /// Adds a button that asks for confirmation when pressed. Only returns <see langword="true"/> once that is acknowledged.
    /// </summary>
    bool confirm_button(const char *label, float width, const char *message, ...);
}
