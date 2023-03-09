/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "res\version.h"
#include "screenshot.hpp"

#include <chrono>
#include <list>

#if !defined(_DEBUG) && ADDON_MAJOR < RESHADE_API_VERSION
#error メジャー バージョンはAPI改訂番号と同一である必要があります。
#endif

class __declspec(uuid("a722aa89-f8e3-43f2-84f2-72fe0122b715")) screenshot_context
{
public:
    screenshot_config config;
    screenshot_environment environment;

    std::chrono::steady_clock::time_point present_time;

    screenshot_myset *active_screenshot = nullptr;

    uint64_t screenshot_begin_frame = std::numeric_limits<decltype(screenshot_begin_frame)>::max();
    uint64_t screenshot_current_frame = 0;
    unsigned int screenshot_repeat_index = 0;
    unsigned int screenshot_total_count = 0;

    std::list<screenshot> screenshots;
    std::atomic<size_t> screenshot_active_threads;
    size_t screenshot_worker_threads = 0;

    bool ignore_shortcuts = false;

    void save();

    inline bool is_screenshot_active() const noexcept;
    inline bool is_screenshot_frame() const noexcept;
    inline bool is_screenshot_frame(screenshot_kind kind) const noexcept;
};
