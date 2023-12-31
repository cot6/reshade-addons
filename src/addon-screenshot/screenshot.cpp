/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-FileCopyrightText: Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "dll_resources.hpp"

#include "runtime_config.hpp"
#include "screenshot.hpp"

#include <time.h>

#include <fpng.h>
#include <png.h>
#include <tiffio.h>
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
        show_osd = decltype(show_osd)::show_osd_while_myset_is_active;
    if (!config.get("SCREENSHOT", "TurnOnEffects", reinterpret_cast<unsigned int &>(turn_on_effects)))
        turn_on_effects = decltype(turn_on_effects)::ignore;

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
    config.set("SCREENSHOT", "TurnOnEffects", static_cast<unsigned int>(turn_on_effects));
}

void screenshot_myset::load(const ini_file &config)
{
    std::string section = ':' + name;

    if (!config.get(section, "AfterImage", after_image))
        after_image.clear();
    if (!config.get(section, "BeforeImage", before_image))
        before_image.clear();
    if (!config.get(section, "ImageFormat", image_format))
        image_format = 0;
    if (!config.get(section, "KeyScreenshot", screenshot_key_data))
        std::memset(screenshot_key_data, 0, sizeof(screenshot_key_data));
    if (!config.get(section, "OriginalImage", original_image))
        original_image.clear();
    if (!config.get(section, "OverlayImage", overlay_image))
        overlay_image.clear();
    if (!config.get(section, "DepthImage", depth_image))
        depth_image.clear();
    if (!config.get(section, "RepeatCount", repeat_count))
        repeat_count = 1;
    if (!config.get(section, "RepeatInterval", repeat_interval))
        repeat_interval = 60;
    if (!config.get(section, "WorkerThreads", worker_threads))
        worker_threads = 0;
    if (!config.get(section, "SoundPath", playsound_path))
        playsound_path.clear();
    if (!config.get(section, "PlaybackMode", reinterpret_cast<unsigned int &>(playback_mode)))
        playback_mode = playback_first_time_only;
    if (!config.get(section, "PlayDefaultIfNotExist", playsound_force))
        playsound_force = false;
    if (!config.get(section, "PlaySoundAsSystemNotification", playsound_as_system_notification))
        playsound_as_system_notification = true;
}
void screenshot_myset::save(ini_file &config) const
{
    std::string section = ':' + name;

    config.set(section, "AfterImage", after_image);
    config.set(section, "BeforeImage", before_image);
    config.set(section, "ImageFormat", image_format);
    config.set(section, "KeyScreenshot", screenshot_key_data);
    config.set(section, "OriginalImage", original_image);
    config.set(section, "OverlayImage", overlay_image);
    config.set(section, "DepthImage", depth_image);
    config.set(section, "RepeatCount", repeat_count);
    config.set(section, "RepeatInterval", repeat_interval);
    config.set(section, "WorkerThreads", worker_threads);
    config.set(section, "SoundPath", playsound_path);
    config.set(section, "PlaybackMode", static_cast<unsigned int>(playback_mode));
    config.set(section, "PlayDefaultIfNotExist", playsound_force);
    config.set(section, "PlaySoundAsSystemNotification", playsound_as_system_notification);
}

void screenshot_environment::load(reshade::api::effect_runtime *runtime)
{
    constexpr size_t SIZE = sizeof(wchar_t) * 4096;
    union
    {
        char as_char[SIZE / sizeof(char)];
        wchar_t as_wchar[SIZE / sizeof(wchar_t)];
    } buf{};
    std::error_code ec{};
    size_t size;

    size = ExpandEnvironmentStringsW(L"%ALLUSERSPROFILE%", buf.as_wchar, ARRAYSIZE(buf.as_wchar));
    addon_private_path = std::wstring(buf.as_wchar, size > 0 ? size - 1 : 0);
    addon_private_path = addon_private_path / L"ReShade Addons" / L"Seri" / L"Screenshot" / L"reshade-shaders" / L"Shaders";
    std::filesystem::create_directories(addon_private_path, ec);

    if (size = ARRAYSIZE(buf.as_char), reshade::get_reshade_base_path(buf.as_char, &size); size != 0)
        reshade_base_path = std::filesystem::u8path(std::string(buf.as_char, size));
    else
        reshade_base_path.clear();

    if (size = GetModuleFileNameW(nullptr, buf.as_wchar, ARRAYSIZE(buf.as_wchar)); size != 0)
        reshade_executable_path = std::wstring(buf.as_wchar, size);
    else
        reshade_executable_path.clear();

    if (runtime != nullptr)
    {
        if (size = ARRAYSIZE(buf.as_char), runtime->get_current_preset_path(buf.as_char, &size); size)
            reshade_preset_path = std::filesystem::u8path(std::string(buf.as_char, size));
        else
            reshade_preset_path.clear();
    }

    addon_screenshot_config_path = reshade_base_path / L"ReShade_Add-On_Screenshot.ini";
}
void screenshot_environment::init()
{
    std::error_code ec{};
    if (std::filesystem::create_directories(addon_private_path, ec); ec.value() == 0)
    {
        const std::filesystem::path copy_to = addon_private_path / L"__Addon_ScreenshotDepth_Seri14.addonfx";
        if (std::filesystem::path copy_from = addon_private_path / L"__Addon_ScreenshotDepth_Seri14.addonfxgen";
            std::filesystem::exists(copy_from))
        {
            std::filesystem::copy_file(copy_from, copy_to, std::filesystem::copy_options::overwrite_existing, ec);
        }
        else
        {
            const reshade::resources::data_resource resource = reshade::resources::load_data_resource(IDR_SCREENSHOTDEPTH_ADDONFX);

            enum class condition { none, open, create, blocked };
            auto condition = condition::none;

            const HANDLE file = CreateFileW(copy_to.c_str(), FILE_GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);

            if (file == INVALID_HANDLE_VALUE)
                condition = condition::blocked;
            else if (GetLastError() == ERROR_ALREADY_EXISTS)
                condition = condition::open;
            else
                condition = condition::create;

            if (condition == condition::open || condition == condition::create)
            {
                if (DWORD _; WriteFile(file, resource.data, static_cast<DWORD>(resource.data_size), &_, NULL) != 0)
                    SetEndOfFile(file);
            }

            if (file != INVALID_HANDLE_VALUE)
                CloseHandle(file);
        }
    }

    auto try_update_config = [this](const std::filesystem::path &file, bool force = false) -> bool
        {
            ini_file config(file);

            if (!force && !config.has("GENERAL", "EffectSearchPaths"))
                return false;

            std::vector<std::filesystem::path> effect_search_paths;
            config.get("GENERAL", "EffectSearchPaths", effect_search_paths);

            if (std::find_if(effect_search_paths.cbegin(), effect_search_paths.cend(),
                [this](const std::filesystem::path &path) { std::error_code ec; return std::filesystem::equivalent(path, addon_private_path, ec); }) == effect_search_paths.cend())
            {
                effect_search_paths.push_back(addon_private_path);
                config.set("GENERAL", "EffectSearchPaths", effect_search_paths);
                config.flush_cache();

                return true;
            }

            return false;
        };

    bool updated = false;
    for (const std::filesystem::path &file : std::filesystem::directory_iterator(reshade_base_path, std::filesystem::directory_options::skip_permission_denied, ec))
    {
        if (file.extension() != L".ini")
            continue;
        if (!try_update_config(file))
            continue;

        updated = true;
    }
    if (!updated)
        try_update_config(reshade_base_path / L"ReShade.ini", true);
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
        case screenshot_kind::depth:
            image_file = myset.depth_image;
            break;
        default:
            reshade::log_message(reshade::log_level::debug, std::format("Unknown screenshot kind: %d", kind).c_str());
            return;
    }

    image_file = std::filesystem::u8path(expand_macro_string(image_file.u8string()));
    image_file = environment.reshade_base_path / image_file;

    if (image_file.has_parent_path())
        std::filesystem::create_directory(image_file.parent_path(), ec);

    if (kind == screenshot_kind::depth)
    {
        int tif_ec = 0;
        image_file.replace_extension() += L".tiff";

        if (TIFF *tif = TIFFOpenW(image_file.native().c_str(), "wl");
            tif != nullptr)
        {
            TIFFWriteBufferSetup(tif, nullptr, std::min<tmsize_t>(static_cast<size_t>(1024 * 256), pixels.size()));

            // 256 - 259
            TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, static_cast<uint16_t>(width));
            TIFFSetField(tif, TIFFTAG_IMAGELENGTH, static_cast<uint16_t>(height));
            TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, (uint16_t)32);
            TIFFSetField(tif, TIFFTAG_COMPRESSION, (uint16_t)COMPRESSION_LZW);

            // 262
            TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, (uint16_t)PHOTOMETRIC_MINISBLACK);

            // 263
            TIFFSetField(tif, TIFFTAG_THRESHHOLDING, (uint16_t)THRESHHOLD_BILEVEL);

            // 266
            TIFFSetField(tif, TIFFTAG_FILLORDER, (uint16_t)FILLORDER_MSB2LSB);

            // 274
            TIFFSetField(tif, TIFFTAG_ORIENTATION, (uint16_t)ORIENTATION_TOPLEFT);

            // 277 - 278
            TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, (uint16_t)1);
            TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, static_cast<uint16_t>(height));

            // 282-284
            TIFFSetField(tif, TIFFTAG_XRESOLUTION, (uint16_t)96);
            TIFFSetField(tif, TIFFTAG_YRESOLUTION, (uint16_t)96);
            TIFFSetField(tif, TIFFTAG_PLANARCONFIG, (uint16_t)PLANARCONFIG_CONTIG);

            // 296
            TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, (uint16_t)RESUNIT_INCH);

            // 305
            TIFFSetField(tif, TIFFTAG_SOFTWARE, "ReShade Screenshot Add-on");

            // 306
            const time_t timestamp = std::chrono::system_clock::to_time_t(frame_time);
            if (tm utc; _gmtime64_s(&utc, &timestamp) == 0)
                tif_ec = TIFFSetField(tif, TIFFTAG_DATETIME, std::format("%04d:%02d:%02d %02d:%02d:%02d", 1900 + utc.tm_year, 1 + utc.tm_mon, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec).c_str());

            // 317
            TIFFSetField(tif, TIFFTAG_PREDICTOR, (uint16_t)PREDICTOR_FLOATINGPOINT);

            // 339
            TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, (uint16_t)SAMPLEFORMAT_IEEEFP);

            const size_t row_strip_length = static_cast<size_t>(4) * width;
            uint8_t *buf = pixels.data();
            for (uint32_t row = 0; row < height; ++row, buf += row_strip_length)
                TIFFWriteScanline(tif, buf, row, static_cast<uint16_t>(row_strip_length));

            TIFFClose(tif);
        }
    }
    else if (myset.image_format == 0 || myset.image_format == 1)
    {
        image_file.replace_extension() += L".png";

        FILE *file = nullptr;
        if (errno_t fopen_error = _wfopen_s(&file, image_file.native().c_str(), L"wb");
            file != nullptr)
        {
            const unsigned int channels = myset.image_format == 0 ? 3 : 4;
            const unsigned int size = width * height;

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
                setvbuf(file, nullptr, _IOFBF, static_cast<size_t>(1024 * 768));

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
                        rows[y] = pixel + static_cast<size_t>(channels) * width * y;
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
    else if (myset.image_format == 2 || myset.image_format == 3)
    {
        image_file.replace_extension() += L".png";

        const unsigned int channels = myset.image_format == 2 ? 3 : 4;
        const unsigned int size = width * height;

        uint8_t *const pixel = pixels.data();
        if (channels == 3)
        {
            for (size_t i = 0; i < size; i++)
                *((uint32_t *)&pixel[3 * i]) =
                *((uint32_t *)&pixel[4 * i]);
        }

        if (std::vector<uint8_t> encoded_pixels;
            fpng::fpng_encode_image_to_memory(pixels.data(), width, height, channels, encoded_pixels))
        {
            enum class condition { none, open, create, blocked };
            auto condition = condition::none;

            const HANDLE file = CreateFileW(image_file.c_str(), FILE_GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_ARCHIVE | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

            if (file == INVALID_HANDLE_VALUE)
                condition = condition::blocked;
            else if (GetLastError() == ERROR_ALREADY_EXISTS)
                condition = condition::open;
            else
                condition = condition::create;

            if (condition == condition::open || condition == condition::create)
            {
                if (DWORD _; WriteFile(file, encoded_pixels.data(), static_cast<DWORD>(encoded_pixels.size()), &_, NULL) != 0)
                    SetEndOfFile(file);
            }

            if (file != INVALID_HANDLE_VALUE)
                CloseHandle(file);
        }
        else
        {
            reshade::log_message(reshade::log_level::error, "Failed to save screenshot: fpng_encode_image_to_memory()");
        }
    }
    else if (myset.image_format == 4 || myset.image_format == 5)
    {
        int tif_ec = 0;
        image_file.replace_extension() += L".tiff";

        const unsigned int channels = myset.image_format == 4 ? 3 : 4;
        const unsigned int size = width * height;

        uint8_t *const pixel = pixels.data();
        if (channels == 3)
        {
            for (size_t i = 0; i < size; i++)
                *((uint32_t *)&pixel[3 * i]) =
                *((uint32_t *)&pixel[4 * i]);
        }

        if (TIFF *tif = TIFFOpenW(image_file.native().c_str(), "wl");
            tif != nullptr)
        {
            TIFFWriteBufferSetup(tif, nullptr, std::min<tmsize_t>(static_cast<size_t>(1024 * 768), pixels.size()));

            // 256 - 259
            TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, static_cast<uint16_t>(width));
            TIFFSetField(tif, TIFFTAG_IMAGELENGTH, static_cast<uint16_t>(height));
            TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, (uint16_t)8);
            TIFFSetField(tif, TIFFTAG_COMPRESSION, (uint16_t)COMPRESSION_LZW);

            // 262
            TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, (uint16_t)PHOTOMETRIC_RGB);

            // 266
            TIFFSetField(tif, TIFFTAG_FILLORDER, (uint16_t)FILLORDER_MSB2LSB);

            // 274
            TIFFSetField(tif, TIFFTAG_ORIENTATION, (uint16_t)ORIENTATION_TOPLEFT);

            // 277 - 278
            TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, static_cast<uint16_t>(channels));
            TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, static_cast<uint16_t>(height));

            // 282-284
            TIFFSetField(tif, TIFFTAG_XRESOLUTION, (uint16_t)96);
            TIFFSetField(tif, TIFFTAG_YRESOLUTION, (uint16_t)96);
            TIFFSetField(tif, TIFFTAG_PLANARCONFIG, (uint16_t)PLANARCONFIG_CONTIG);

            // 296
            TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, (uint16_t)RESUNIT_INCH);

            // 305
            TIFFSetField(tif, TIFFTAG_SOFTWARE, "ReShade Screenshot Add-on");

            // 306
            const time_t timestamp = std::chrono::system_clock::to_time_t(frame_time);
            if (tm utc; _gmtime64_s(&utc, &timestamp) == 0)
                tif_ec = TIFFSetField(tif, TIFFTAG_DATETIME, std::format("%04d:%02d:%02d %02d:%02d:%02d", 1900 + utc.tm_year, 1 + utc.tm_mon, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec).c_str());

            // 317
            TIFFSetField(tif, TIFFTAG_PREDICTOR, (uint16_t)PREDICTOR_HORIZONTAL);

            // 338
            if (channels == 4)
            {
                uint16_t v[1] = { (uint16_t)EXTRASAMPLE_UNASSALPHA };
                TIFFSetField(tif, TIFFTAG_EXTRASAMPLES, (uint16_t)1, v);
            }

            // 339
            TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, (uint16_t)SAMPLEFORMAT_UINT);

            const unsigned int row_strip_length = channels * width;
            uint8_t *buf = pixels.data();
            for (uint32_t row = 0; row < height; ++row, buf += row_strip_length)
                TIFFWriteScanline(tif, buf, row, static_cast<uint16_t>(row_strip_length));

            TIFFClose(tif);
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

    for (size_t offset = 0, macro_beg = std::string::npos, macro_end = std::string::npos;
        offset < input.size();
        offset = macro_end + 1)
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
