// SPDX-FileCopyrightText: 2018 seri14
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#include "res\version.h"

#include <reshade.hpp>

#if !defined(_DEBUG) && ADDON_MAJOR < RESHADE_API_VERSION
// The major version must be the same as the API revision number.
#endif

extern "C" __declspec(dllexport) const char *WEBSITE = "https://github.com/cot6/reshade-addons";
extern "C" __declspec(dllexport) const char *ISSUES = "https://github.com/cot6/reshade-addons/issues";
