/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>
#include <vector>

#include "res\version.h"

#include <reshade.hpp>

#if !defined(_DEBUG) && ADDON_MAJOR < RESHADE_API_VERSION
// The major version must be the same as the API revision number.
#endif

struct __declspec(uuid("a0ca6a72-49f2-4440-9260-bed0f46263d2")) adjust_context
{
    reshade::api::effect_uniform_variable ignore_preprocessor_definitions_variable;
    std::vector<std::pair<reshade::api::effect_uniform_variable, std::string>> displaydepth_variables;
    std::vector<std::pair<reshade::api::effect_uniform_variable, std::string>> saving_variables;
    bool ignore_preprocessor_definitions;
    bool saving_preprocessor_definitions;
};
