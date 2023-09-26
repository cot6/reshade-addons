/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "res\version.h"
#include "runtime_config.hpp"

#include <reshade.hpp>

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
    unsigned int repeat_wait = 60;
    unsigned int screenshot_key_data[4]{ 0, 0, 0, 0 };

    std::filesystem::path original_image;
    std::filesystem::path before_image;
    std::filesystem::path after_image;
    std::filesystem::path overlay_image;
    std::filesystem::path depth_image;

    std::list<std::pair<std::string, std::filesystem::path>> technique_images;

    unsigned int worker_threads = 0;

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
        always,
        while_myset_is_active,
    } show_osd = hidden;

    void load(const ini_file &config);
    void save(ini_file &config, bool header_only = false);
};

class screenshot_environment
{
public:
    const unsigned int thread_hardware_concurrency = std::thread::hardware_concurrency();

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

    screenshot() = default;
    screenshot(screenshot &&screenshot) = default;
    screenshot(reshade::api::effect_runtime *runtime, const screenshot_environment &environment, const screenshot_myset &myset, screenshot_kind kind, screenshot_state &state) :
        environment(environment),
        myset(myset),
        kind(kind),
        state(state)
    {
        capture(runtime);
    }

    void capture(reshade::api::effect_runtime *runtime)
    {
        frame_time = std::chrono::system_clock::now();
        runtime->get_screenshot_width_and_height(&width, &height);

        switch (kind)
        {
            case screenshot_kind::original:
            case screenshot_kind::before:
            case screenshot_kind::after:
            case screenshot_kind::overlay:
                pixels.resize(width * height * 4);
                runtime->capture_screenshot(pixels.data());
                break;
            default:
            case screenshot_kind::depth:
                break;
        }
    }

    void save();

private:
    std::string expand_macro_string(const std::string &input);
};

