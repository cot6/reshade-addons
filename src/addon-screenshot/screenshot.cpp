/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-FileCopyrightText: Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "runtime_config.hpp"
#include "screenshot.hpp"

#include <png.h>
#include <zlib.h>

#include <functional>
#include <list>
#include <string>
#include <vector>

void screenshot_config::load(const ini_file &config)
{
    std::string preset_names;
    if (!config.get("SCREENSHOT", "PresetNames", preset_names))
        preset_names.clear();
    if (!config.get("OVERLAY", "ShowOSD", reinterpret_cast<unsigned int &>(show_osd)))
        show_osd = decltype(show_osd)::while_myset_is_active;

    for (size_t seek = 0; seek < preset_names.size();)
    {
        size_t size = preset_names.find('&', seek);
        if (size == std::string::npos)
            size = preset_names.size() - seek;

        screenshot_mysets.emplace_back(config, preset_names.substr(seek, size));
        seek += size + 1;
    }
}
void screenshot_config::save(ini_file &config, bool header_only)
{
    std::string preset_names;

    for (screenshot_myset &myset : screenshot_mysets)
    {
        if (!header_only)
            myset.save(config);

        preset_names += myset.name + '&';
    }

    if (!preset_names.empty())
        preset_names.resize(preset_names.size() - 1);

    config.set("SCREENSHOT", "PresetNames", preset_names);
    config.set("OVERLAY", "ShowOSD", static_cast<unsigned int>(show_osd));
}

void screenshot_myset::load(const ini_file &config)
{
    std::string section = ':' + name;

    if (!config.get(section.c_str(), "AfterImage", after_image))
        after_image.clear();
    if (!config.get(section.c_str(), "BeforeImage", before_image))
        before_image.clear();
    if (!config.get(section.c_str(), "ImageFormat", image_format))
        image_format = 0;
    if (!config.get(section.c_str(), "KeyScreenshot", screenshot_key_data))
        std::memset(screenshot_key_data, 0, sizeof(screenshot_key_data));
    if (!config.get(section.c_str(), "OriginalImage", original_image))
        original_image.clear();
    if (!config.get(section.c_str(), "OverlayImage", overlay_image))
        overlay_image.clear();
    if (!config.get(section.c_str(), "RepeatCount", repeat_count))
        repeat_count = 1;
    if (!config.get(section.c_str(), "RepeatWait", repeat_wait))
        repeat_wait = 60;
    if (!config.get(section.c_str(), "WorkerThreads", worker_threads))
        worker_threads = 0;
}
void screenshot_myset::save(ini_file &config) const
{
    std::string section = ':' + name;

    config.set(section.c_str(), "AfterImage", after_image);
    config.set(section.c_str(), "BeforeImage", before_image);
    config.set(section.c_str(), "ImageFormat", image_format);
    config.set(section.c_str(), "KeyScreenshot", screenshot_key_data);
    config.set(section.c_str(), "OriginalImage", original_image);
    config.set(section.c_str(), "OverlayImage", overlay_image);
    config.set(section.c_str(), "RepeatCount", repeat_count);
    config.set(section.c_str(), "RepeatWait", repeat_wait);
    config.set(section.c_str(), "WorkerThreads", worker_threads);
}

void screenshot_environment::load(reshade::api::effect_runtime *runtime)
{
    constexpr size_t SIZE = sizeof(wchar_t) * 4096;
    union
    {
        char as_char[SIZE / sizeof(char)];
        wchar_t as_wchar[SIZE / sizeof(wchar_t)];
    } buf{};
    size_t size;

    if (size = ARRAYSIZE(buf.as_char), reshade::get_reshade_base_path(buf.as_char, &size); size != 0)
        reshade_base_path = std::filesystem::u8path(std::string(buf.as_char, size));
    else
        reshade_base_path.clear();

    if (size = GetModuleFileNameW(nullptr, buf.as_wchar, ARRAYSIZE(buf.as_wchar)); size != 0)
        reshade_executable_path = std::wstring(buf.as_wchar, size);
    else
        reshade_executable_path.clear();

    if (size = ARRAYSIZE(buf.as_char), runtime->get_current_preset_path(buf.as_char, &size); size)
        reshade_preset_path = std::filesystem::u8path(std::string(buf.as_char, size));
    else
        reshade_preset_path.clear();

    addon_screenshot_config_path = reshade_base_path / L"ReShade_Add-On_Screenshot.ini";
}

void screenshot::save()
{
    std::error_code ec{};

    std::filesystem::path image_file;
    switch (kind)
    {
        case screenshot_kind::after:
            image_file = myset.after_image;
            break;
        case screenshot_kind::before:
            image_file = myset.before_image;
            break;
        case screenshot_kind::original:
            image_file = myset.original_image;
            break;
        case screenshot_kind::overlay:
            image_file = myset.overlay_image;
            break;
        default:
            reshade::log_message(reshade::log_level::debug, std::format("Unknown screenshot kind: %d", kind).c_str());
            return;
    }

    image_file = std::filesystem::u8path(expand_macro_string(image_file.u8string()));
    image_file = environment.reshade_base_path / image_file;

    if (image_file.has_parent_path())
        std::filesystem::create_directory(image_file.parent_path(), ec);

    if (myset.image_format == 0 || myset.image_format == 1)
    {
        image_file.replace_extension() += L".png";

        FILE *file = nullptr;
        if (errno_t fopen_error = _wfopen_s(&file, image_file.native().c_str(), L"wb");
            file != nullptr)
        {
            const size_t channels = myset.image_format == 0 ? 3 : 4;
            const size_t size = static_cast<size_t>(width) * height;

            uint8_t *const pixel = pixels.data();
            if (channels == 3)
            {
                for (size_t i = 0; i < size; i++)
                    *((uint32_t *)&pixel[3 * i]) =
                    *((uint32_t *)&pixel[4 * i]);
            }

            png_structp write_ptr = nullptr;
            png_infop info_ptr = nullptr;

            if (write_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
                write_ptr != nullptr)
            {
                setvbuf(file, nullptr, _IOFBF, 1024 * 512);

                png_init_io(write_ptr, file);
                png_set_filter(write_ptr, PNG_FILTER_TYPE_BASE, PNG_ALL_FILTERS);

                png_set_compression_mem_level(write_ptr, MAX_MEM_LEVEL);
                png_set_compression_buffer_size(write_ptr, 65536);

                png_set_compression_level(write_ptr, Z_BEST_COMPRESSION);
                png_set_compression_strategy(write_ptr, Z_RLE);

                if (info_ptr = png_create_info_struct(write_ptr);
                    info_ptr != nullptr)
                {
                    png_set_IHDR(write_ptr, info_ptr, width, height, 8, myset.image_format == 0 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

                    png_time mod_time{};
                    png_convert_from_time_t(&mod_time, std::chrono::system_clock::to_time_t(frame_time));
                    png_set_tIME(write_ptr, info_ptr, &mod_time);

                    png_write_info(write_ptr, info_ptr);

                    std::vector<png_bytep> rows(height);
                    for (size_t y = 0; y < height; y++)
                        rows[y] = pixel + channels * width * y;
                    png_write_image(write_ptr, rows.data());

                    png_write_end(write_ptr, info_ptr);
                }
            }

            png_destroy_write_struct(&write_ptr, &info_ptr);
            png_destroy_info_struct(write_ptr, &info_ptr);

            fclose(file);
        }
        else
        {
            char buffer[BUFSIZ]{};
            if (strerror_s(buffer, fopen_error) == 0)
                reshade::log_message(reshade::log_level::error, std::format("Failed to save screenshot: %s (%d)", buffer, fopen_error).c_str());

            state.error_occurs++;
        }
    }

    return;
}

std::string screenshot::expand_macro_string(const std::string &input)
{
    std::list<std::pair<std::string, std::function<std::string(std::string_view)>>> macros;

    macros.emplace_back("APP", [this](std::string_view) { return environment.reshade_executable_path.stem().u8string(); });
    macros.emplace_back("PRESET", [this](std::string_view) { return environment.reshade_preset_path.stem().u8string(); });
    macros.emplace_back("INDEX",
        [this](std::string_view fmt) {
            if (fmt.empty())
                fmt = "D1";
            bool zeroed = false;
            if (fmt[0] == 'D' || fmt[0] == 'd')
                zeroed = true;
            int digits = 1;
            if (fmt.size() == 1 && '1' <= fmt[0] && fmt[0] <= '9')
                digits = fmt[0] - '0';
            if (fmt.size() == 2 && '1' <= fmt[1] && fmt[1] <= '9')
                digits = fmt[1] - '0';
            return std::format(zeroed ? "%0*u" : "%*u", digits, repeat_index);
        });

    macros.emplace_back("DATE",
        [this](std::string_view fmt) {
            const std::time_t t = std::chrono::system_clock::to_time_t(frame_time);
            struct tm tm; localtime_s(&tm, &t);
            if (fmt.empty())
                fmt = "%Y-%m-%d %H-%M-%S";
            char str[128] = "";
            const std::string tailzeroed_fmt = std::string(fmt);
            size_t len = strftime(str, ARRAYSIZE(str), tailzeroed_fmt.c_str(), &tm);
            return std::string(str, len);
        });

    std::string result;

    for (size_t offset = 0, macro_beg, macro_end; offset < input.size(); offset = macro_end + 1)
    {
        macro_beg = input.find('<', offset);
        macro_end = input.find('>', macro_beg + 1);

        if (macro_beg == std::string::npos || macro_end == std::string::npos)
        {
            result += input.substr(offset);
            break;
        }
        else
        {
            result += input.substr(offset, macro_beg - offset);
        }

        std::string_view replacing = std::string_view(input).substr(macro_beg + 1, macro_end - (macro_beg + 1));
        size_t colon_pos = replacing.find(':');

        std::string name;
        if (colon_pos == std::string::npos)
            name = replacing;
        else
            name = replacing.substr(0, colon_pos);

        std::string value;

        for (const auto &macro : macros)
        {
            if (_stricmp(name.c_str(), macro.first.c_str()) == 0)
            {
                std::string_view fmt;
                if (colon_pos != std::string::npos)
                    fmt = replacing.substr(colon_pos + 1);
                value = macro.second(fmt);
                break;
            }
        }

        result += value;
    }

    return result;
}
