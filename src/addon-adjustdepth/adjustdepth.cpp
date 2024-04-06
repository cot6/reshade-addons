/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "adjustdepth.hpp"

void adjustdepth_config::load(const ini_file &config)
{
    std::vector<std::string> sections;
    config.get("SHORTCUT", sections);
    for (std::string &section : sections)
    {
        auto &shortcut = shortcuts.emplace_back();
        config.get("SHORTCUT", section, shortcut.overwrite_key_data);
        config.get(section, shortcut.definitions);
        shortcut.section = std::move(section);
    }
}
void adjustdepth_config::save(ini_file &config, bool header_only)
{
    for (const adjustdepth_shortcut &shortcut : shortcuts)
    {
        config.set("SHORTCUT", shortcut.section, shortcut.overwrite_key_data);
        config.set(shortcut.section, shortcut.definitions);
    }

    std::vector<std::string> sections;
    config.get("SHORTCUT", sections);
    for (const std::string &section : sections)
    {
        if (std::find_if(shortcuts.begin(), shortcuts.end(), [&section](adjustdepth_shortcut &shortcut) { return section == shortcut.section; }) == shortcuts.end())
        {
            config.erase("SHORTCUT", section);
            config.erase(section);
        }
    }
}

void adjustdepth_environment::load(reshade::api::effect_runtime *runtime)
{
    constexpr size_t SIZE = sizeof(wchar_t) * 4096;
    union
    {
        char as_char[SIZE / sizeof(char)];
        wchar_t as_wchar[SIZE / sizeof(wchar_t)];
    } buf{};
    std::error_code ec{};
    size_t size;

    if (size = ARRAYSIZE(buf.as_char), reshade::get_reshade_base_path(buf.as_char, &size); size != 0)
        reshade_base_path = std::filesystem::u8path(std::string_view(buf.as_char, size));
    else
        reshade_base_path.clear();

    if (size = GetModuleFileNameW(nullptr, buf.as_wchar, ARRAYSIZE(buf.as_wchar)); size != 0)
        reshade_executable_path = std::wstring_view(buf.as_wchar, size);
    else
        reshade_executable_path.clear();

    if (runtime != nullptr)
    {
        if (size = ARRAYSIZE(buf.as_char), runtime->get_current_preset_path(buf.as_char, &size); size)
            reshade_preset_path = std::filesystem::u8path(std::string_view(buf.as_char, size));
        else
            reshade_preset_path.clear();
    }

    addon_adjustdepth_config_path = reshade_base_path / L"ReShade_Addon_AdjustDepth.ini";
}
