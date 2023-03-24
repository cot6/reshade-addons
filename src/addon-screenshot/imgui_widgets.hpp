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
	bool key_input_box(const char *name, unsigned int(&key)[4], reshade::api::effect_runtime *runtime);
}
