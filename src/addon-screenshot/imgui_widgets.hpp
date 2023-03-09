/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <imgui.h>
#include <reshade.hpp>

bool key_input_box(const char *name, unsigned int(&key)[4], reshade::api::effect_runtime *runtime);
