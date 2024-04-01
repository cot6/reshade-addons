/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "res\version.h"
#include "screenshot.hpp"

#include <reshade.hpp>

#include <chrono>
#include <list>

#if !defined(_DEBUG) && ADDON_MAJOR < RESHADE_API_VERSION
// The major version must be the same as the API revision number.
#endif
constexpr uint8_t s_runtime_id[16] = { 0x02, 0x82, 0xFF, 0x77, /**/ 0xEC, 0x5B, /**/ 0xAD, 0x42, /**/ 0x8C, 0xE0, 0x39, 0x7F, 0x3E, 0x84, 0xEA, 0xA6 };

class __declspec(uuid("a722aa89-f8e3-43f2-84f2-72fe0122b715")) screenshot_context
{
public:
    screenshot_config config;
    screenshot_environment environment;

    uint64_t current_frame = 0;

    std::chrono::system_clock::time_point present_time;
    std::chrono::system_clock::time_point capture_time, capture_last;

    screenshot_myset *active_screenshot = nullptr;
    screenshot_state screenshot_state;

    uint64_t screenshot_begin_frame = std::numeric_limits<decltype(screenshot_begin_frame)>::max();
    unsigned int screenshot_repeat_index = 0;

    std::list<screenshot> screenshots;
    std::atomic<size_t> screenshot_active_threads;
    size_t screenshot_worker_threads = 0;

    bool ignore_shortcuts = false;
    bool effects_state_activated = false;

    DWORD playsound_flags = 0;

    void save();

    inline bool is_screenshot_active() const noexcept;
    inline bool is_screenshot_enable(screenshot_kind kind) const noexcept;
    inline bool is_screenshot_frame() const noexcept;
    inline bool is_screenshot_frame(screenshot_kind kind) const noexcept;

    reshade::api::effect_technique screenshotdepth_technique {};
};
