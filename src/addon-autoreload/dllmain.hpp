/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "res\version.h"
#include "autoreload.hpp"

#include <cstdint>
#include <string>

#include <reshade.hpp>

#if !defined(_DEBUG) && ADDON_MAJOR < RESHADE_API_VERSION
// The major version must be the same as the API revision number.
#endif

extern "C" __declspec(dllexport) const char *WEBSITE = "https://github.com/cot6/reshade-addons";
extern "C" __declspec(dllexport) const char *ISSUES = "https://github.com/cot6/reshade-addons/issues";

constexpr uint8_t s_runtime_id[16] = { 0x02, 0x82, 0xFF, 0x77, /**/ 0xEC, 0x5B, /**/ 0xAD, 0x42, /**/ 0x8C, 0xE0, 0x39, 0x7F, 0x3E, 0x84, 0xEA, 0xA6 };

struct technique_state
{
    std::string name;
    bool enabled = false;
    bool used = false;
    std::vector<std::string> order;
};

class __declspec(uuid("70fbbd42-7153-4a47-b155-c499d68d8d8d")) autoreload_context
{
public:
    autoreload_context() {}
    ~autoreload_context() {}

    filesystem_update_listener _filesystem_update_listener;
    efsw::FileWatcher _file_watcher;
    std::vector<technique_state> _reloading_techniques;

    uint64_t _current_frame = 0;
    uint64_t _reloading_frame = std::numeric_limits<uint64_t>::max();
    uint64_t _reloaded_frame = std::numeric_limits<uint64_t>::max();
};
