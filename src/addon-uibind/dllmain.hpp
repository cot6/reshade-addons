// SPDX-FileCopyrightText: 2018 seri14
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#include "res\version.h"

#include <imgui.h>
#include <reshade.hpp>

#include <string>
#include <tuple>
#include <vector>

#if !defined(_DEBUG) && ADDON_MAJOR < RESHADE_API_VERSION
// The major version must be the same as the API revision number.
#endif

extern "C" __declspec(dllexport) const char *WEBSITE = "https://github.com/cot6/reshade-addons";
extern "C" __declspec(dllexport) const char *ISSUES = "https://github.com/cot6/reshade-addons/issues";

constexpr uint8_t s_runtime_id[16] = { 0x02, 0x82, 0xFF, 0x77, /**/ 0xEC, 0x5B, /**/ 0xAD, 0x42, /**/ 0x8C, 0xE0, 0x39, 0x7F, 0x3E, 0x84, 0xEA, 0xA6 };

struct __declspec(uuid("4c9beed6-ba6a-459c-976b-ca42eca95518")) uibind_context
{
    bool has_reshade_gui = false;
    std::vector<std::tuple<reshade::api::effect_uniform_variable, std::string, std::string, std::string>> applying_preprocessor_definitions;
};
