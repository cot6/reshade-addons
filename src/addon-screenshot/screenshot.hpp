/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <array>
#include <chrono>
#include <filesystem>
#include <thread>
#include <list>
#include <vector>

#include <errno.h>
#include <inttypes.h>

#include <setjmp.h> // This for application must include this before png.h to obtain the definition of jmp_buf.

#include "res\version.h"
#include "runtime_config.hpp"

#include <reshade.hpp>
#include <utf8\unchecked.h>

#include <png.h>
#include <zlib.h>

enum screenshot_kind
{
    unset = 0,
    original = 1,
    before = 2,
    after = 3,
    overlay = 4,
    depth = 5,
    preset = 6,
    _max,
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
    std::atomic<uint64_t> last_elapsed;
    std::atomic<unsigned int> error_occurs;

    void reset()
    {
        error_occurs = 0;
    }
};

struct screenshot_statistics_scoped_data
{
    uint64_t total_take;
    uint64_t total_frame;
};

class screenshot_statistics
{
public:
    std::unordered_map<std::string, screenshot_statistics_scoped_data> capture_counts;

    void load(const ini_file &config);
    void save(ini_file &config) const;
};

class screenshot_myset
{
public:
    std::string name;

    unsigned int image_format = 0;
    unsigned int repeat_count = 1;
    unsigned int repeat_interval = 60;
    unsigned int screenshot_key_data[4]{ 0, 0, 0, 0 };

    std::array<std::filesystem::path, screenshot_kind::_max> image_paths;
    std::array<uint64_t, screenshot_kind::_max> image_freelimits;

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

    int file_write_buffer_size = 1024 * 768;
    int libpng_png_filters = PNG_ALL_FILTERS;
    int zlib_compression_level = Z_BEST_COMPRESSION;
    int zlib_compression_strategy = Z_RLE;

    // Validating

    std::string preset_status;
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
        if (is_muted(kind))
            return false;
        else
            return image_paths[kind].empty() ? unset : kind;
    }
    bool is_muted(screenshot_kind kind) const
    {
        const std::filesystem::path &path = image_paths[kind];
        return !path.empty() && path.native().front() == L'-';
    }
    void mute(screenshot_kind kind)
    {
        if (std::filesystem::path &path = image_paths[kind]; !path.native().empty() && path.native().front() != L'-')
            path = L'-' + path.native();
    }
    void unmute(screenshot_kind kind)
    {
        if (std::filesystem::path &path = image_paths[kind]; !path.native().empty() && path.native().front() == L'-')
            path = path.native().substr(1);
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
        show_osd_while_myset_is_active_ignore_errors,
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
    std::filesystem::path addon_screenshot_statistics_path;

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
    screenshot_statistics statistics;

    screenshot_myset myset;
    screenshot_state &state;

    std::filesystem::path image_file, preset_file;
    std::array<std::filesystem::path, screenshot_kind::_max> image_files;

    unsigned int repeat_index = 0;
    unsigned int height = 0, width = 0;

    std::array<std::vector<uint32_t>, screenshot_kind::_max> captures;
    std::chrono::system_clock::time_point frame_time;

    std::string message;

    screenshot(screenshot &&screenshot) = default;
    screenshot(const screenshot_environment &environment,
               const screenshot_myset &myset,
               screenshot_state &state,
               std::chrono::system_clock::time_point frame_time,
               screenshot_statistics statistics) :
        environment(environment),
        myset(myset),
        state(state),
        frame_time(frame_time),
        statistics(statistics)
    {

    };

    bool capture(reshade::api::effect_runtime *const runtime, screenshot_kind kind);

    void save_preset(reshade::api::effect_runtime *runtime = nullptr);

    void save_image();
    void save_image(screenshot_kind kind);

    std::string expand_macro_string(const std::string &input) const;

    [[noreturn]]
    static void user_error_fn(png_structp png_ptr, png_const_charp error_msg);
    static void user_warning_fn(png_structp png_ptr, png_const_charp warning_msg);
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
