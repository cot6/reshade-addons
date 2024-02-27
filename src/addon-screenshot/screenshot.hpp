/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "res\version.h"
#include "runtime_config.hpp"

#include <reshade.hpp>
#include <utf8\unchecked.h>

#include <chrono>
#include <filesystem>
#include <thread>
#include <list>
#include <vector>

enum screenshot_kind
{
    unset = 0,
    original,
    before,
    after,
    overlay,
    depth,
};

constexpr const char *get_screenshot_kind_name(screenshot_kind kind)
{
    switch (kind)
    {
        case screenshot_kind::original:
            return "original";
        case screenshot_kind::before:
            return "before";
        case screenshot_kind::after:
            return "after";
        case screenshot_kind::overlay:
            return "overlay";
        case screenshot_kind::depth:
            return "depth";
        default:
            return "unknown";
    }
}

class screenshot_state
{
public:
    std::atomic<unsigned int> error_occurs;

    void reset()
    {
        error_occurs = 0;
    }
};

class screenshot_myset
{
public:
    std::string name;

    unsigned int image_format = 0;
    unsigned int repeat_count = 1;
    unsigned int repeat_interval = 60;
    unsigned int screenshot_key_data[4]{ 0, 0, 0, 0 };

    std::filesystem::path original_image;
    std::filesystem::path before_image;
    std::filesystem::path after_image;
    std::filesystem::path overlay_image;
    std::filesystem::path depth_image;

    unsigned int worker_threads = 0;

    std::filesystem::path playsound_path;
    enum : unsigned int
    {
        playback_first_time_only = 0,
        playback_every_time,
        playback_while_myset_is_active,
    } playback_mode = playback_first_time_only;
    bool playsound_force = false;
    bool playsound_as_system_notification = true;

    // Validating

    std::string original_status;
    std::string before_status;
    std::string after_status;
    std::string overlay_status;
    std::string depth_status;

    screenshot_myset() = default;
    screenshot_myset(const ini_file &config, std::string &&name) :
        name(std::move(name))
    {
        load(config);
    }

    bool is_enable(screenshot_kind kind) const
    {
        switch (kind)
        {
            case screenshot_kind::original:
                return !(original_image.empty() || original_image.native().front() == L'-');
            case screenshot_kind::before:
                return !(before_image.empty() || before_image.native().front() == L'-');
            case screenshot_kind::after:
                return !(after_image.empty() || after_image.native().front() == L'-');
            case screenshot_kind::overlay:
                return !(overlay_image.empty() || overlay_image.native().front() == L'-');
            case screenshot_kind::depth:
                return !(depth_image.empty() || depth_image.native().front() == L'-');
            default:
                return false;
        }
    }

    void load(const ini_file &config);
    void save(ini_file &config) const;
};

class screenshot_config
{
public:
    std::list<screenshot_myset> screenshot_mysets;
    enum : unsigned int
    {
        hidden = 0,
        show_osd_always,
        show_osd_while_myset_is_active,
    } show_osd = show_osd_while_myset_is_active;
    enum : unsigned int
    {
        ignore = 0,
        turn_on_while_myset_is_active,
        turn_on_when_activate_myset,
    } turn_on_effects = ignore;

    void load(const ini_file &config);
    void save(ini_file &config, bool header_only = false);
};

class screenshot_environment
{
public:
    std::filesystem::path addon_private_path;
    std::filesystem::path reshade_base_path;
    std::filesystem::path reshade_executable_path;
    std::filesystem::path reshade_preset_path;

    std::filesystem::path addon_screenshot_config_path;

    screenshot_environment() = default;
    screenshot_environment(const screenshot_environment &screenshot_env) = default;
    screenshot_environment(screenshot_environment &&screenshot_env) = default;
    screenshot_environment(reshade::api::effect_runtime *runtime)
    {
        load(runtime);
    }

    void load(reshade::api::effect_runtime *runtime);
    void init();
};

class screenshot
{
public:
    screenshot_environment environment;

    screenshot_myset myset;
    screenshot_state &state;

    screenshot_kind kind = screenshot_kind::unset;
    unsigned int repeat_index = 0;

    unsigned int height = 0, width = 0;
    std::vector<uint8_t> pixels;
    std::chrono::system_clock::time_point frame_time;

    std::string message;

    screenshot() = default;
    screenshot(screenshot &&screenshot) = default;
    screenshot(reshade::api::effect_runtime *runtime, const screenshot_environment &environment, const screenshot_myset &myset, screenshot_kind kind, screenshot_state &state, std::chrono::system_clock::time_point frame_time) :
        environment(environment),
        myset(myset),
        kind(kind),
        state(state),
        frame_time(frame_time)
    {
        if (runtime)
            runtime->get_screenshot_width_and_height(&width, &height);

        switch (kind)
        {
            case screenshot_kind::original:
            case screenshot_kind::before:
            case screenshot_kind::after:
            case screenshot_kind::overlay:
                if (runtime)
                {
                    pixels.resize(static_cast<size_t>(width) * height * 4);
                    runtime->capture_screenshot(pixels.data());
                }
                break;
            default:
            case screenshot_kind::depth:
                break;
        }
    }

    void save();
    std::string expand_macro_string(const std::string &input) const;
};

static std::string format_message(DWORD dwMessageId, DWORD dwLanguageId = 0x409) noexcept
{
    std::string status;
    WCHAR fmbuf[128] = L"";
    DWORD len = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
        nullptr, dwMessageId, dwLanguageId, fmbuf, ARRAYSIZE(fmbuf), nullptr);
    std::wstring_view message(fmbuf, len); status.reserve(len);
    while (message.size() > 0 && message.back() <= L'\x20')
        message = message.substr(0, message.size() - 1);
    utf8::unchecked::utf16to8(message.cbegin(), message.cend(), std::back_inserter(status));

    return status;
}
