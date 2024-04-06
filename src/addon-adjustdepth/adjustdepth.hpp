/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "res\version.h"
#include "runtime_config.hpp"

#include <reshade.hpp>

#if !defined(_DEBUG) && ADDON_MAJOR < RESHADE_API_VERSION
// The major version must be the same as the API revision number.
#endif

class adjustdepth_environment
{
public:
    std::filesystem::path reshade_base_path;
    std::filesystem::path reshade_executable_path;
    std::filesystem::path reshade_preset_path;

    std::filesystem::path addon_adjustdepth_config_path;

    adjustdepth_environment() = default;
    adjustdepth_environment(const adjustdepth_environment &environment) = default;
    adjustdepth_environment(adjustdepth_environment &&environment) = default;
    adjustdepth_environment(reshade::api::effect_runtime *runtime)
    {
        load(runtime);
    }

    void load(reshade::api::effect_runtime *runtime);
};

class adjustdepth_shortcut
{
public:
    std::string section;
    unsigned int overwrite_key_data[4] = {};
    ini_data::table definitions;
};

class adjustdepth_config
{
public:
    std::vector<adjustdepth_shortcut> shortcuts;

    void load(const ini_file &config);
    void save(ini_file &config, bool header_only = false);
};
