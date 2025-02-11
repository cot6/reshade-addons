/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-FileCopyrightText: Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "dll_resources.hpp"

#include "addon.hpp"
#include "std_string_ext.hpp"

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

    if (!config.get(section, "AfterImage", image_paths[screenshot_kind::after]))
        image_paths[screenshot_kind::after].clear();
    if (!config.get(section, "AfterImageDiskFreeLimit", image_freelimits[screenshot_kind::after]))
        image_freelimits[screenshot_kind::after] = 0;
    if (!config.get(section, "BeforeImage", image_paths[screenshot_kind::before]))
        image_paths[screenshot_kind::before].clear();
    if (!config.get(section, "BeforeImageDiskFreeLimit", image_freelimits[screenshot_kind::before]))
        image_freelimits[screenshot_kind::before] = 0;
    if (!config.get(section, "ImageFormat", image_format))
        image_format = 0;
    if (!config.get(section, "KeyScreenshot", screenshot_key_data))
        std::memset(screenshot_key_data, 0, sizeof(screenshot_key_data));
    if (!config.get(section, "OriginalImage", image_paths[screenshot_kind::original]))
        image_paths[screenshot_kind::original].clear();
    if (!config.get(section, "OriginalImageDiskFreeLimit", image_freelimits[screenshot_kind::original]))
        image_freelimits[screenshot_kind::original] = 0;
    if (!config.get(section, "OverlayImage", image_paths[screenshot_kind::overlay]))
        image_paths[screenshot_kind::overlay].clear();
    if (!config.get(section, "OverlayImageDiskFreeLimit", image_freelimits[screenshot_kind::overlay]))
        image_freelimits[screenshot_kind::overlay] = 0;
    if (!config.get(section, "DepthImage", image_paths[screenshot_kind::depth]))
        image_paths[screenshot_kind::depth].clear();
    if (!config.get(section, "DepthImageDiskFreeLimit", image_freelimits[screenshot_kind::depth]))
        image_freelimits[screenshot_kind::depth] = 0;
    if (!config.get(section, "PresetSave", image_paths[screenshot_kind::preset]))
        image_paths[screenshot_kind::preset].clear();
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
    if (!config.get(section, "FileWriteBufferSize", file_write_buffer_size))
        file_write_buffer_size = 1024 * 768;
    if (!config.get(section, "LibpngPngFilters", libpng_png_filters))
        libpng_png_filters = PNG_ALL_FILTERS;
    if (!config.get(section, "ZlibCompressionLevel", zlib_compression_level))
        zlib_compression_level = Z_BEST_COMPRESSION;
    if (!config.get(section, "ZlibCompressionStrategy", zlib_compression_strategy))
        zlib_compression_strategy = Z_RLE;
}
void screenshot_myset::save(ini_file &config) const
{
    std::string section = ':' + name;

    config.set(section, "AfterImage", image_paths[screenshot_kind::after]);
    config.set(section, "AfterImageDiskFreeLimit", image_freelimits[screenshot_kind::after]);
    config.set(section, "BeforeImage", image_paths[screenshot_kind::before]);
    config.set(section, "BeforeImageDiskFreeLimit", image_freelimits[screenshot_kind::before]);
    config.set(section, "ImageFormat", image_format);
    config.set(section, "KeyScreenshot", screenshot_key_data);
    config.set(section, "OriginalImage", image_paths[screenshot_kind::original]);
    config.set(section, "OriginalImageDiskFreeLimit", image_freelimits[screenshot_kind::original]);
    config.set(section, "OverlayImage", image_paths[screenshot_kind::overlay]);
    config.set(section, "OverlayImageDiskFreeLimit", image_freelimits[screenshot_kind::overlay]);
    config.set(section, "DepthImage", image_paths[screenshot_kind::depth]);
    config.set(section, "DepthImageDiskFreeLimit", image_freelimits[screenshot_kind::depth]);
    config.set(section, "PresetSave", image_paths[screenshot_kind::preset]);
    config.set(section, "RepeatCount", repeat_count);
    config.set(section, "RepeatInterval", repeat_interval);
    config.set(section, "WorkerThreads", worker_threads);
    config.set(section, "SoundPath", playsound_path);
    config.set(section, "PlaybackMode", static_cast<unsigned int>(playback_mode));
    config.set(section, "PlayDefaultIfNotExist", playsound_force);
    config.set(section, "PlaySoundAsSystemNotification", playsound_as_system_notification);
    config.set(section, "FileWriteBufferSize", file_write_buffer_size);
    config.set(section, "LibpngPngFilters", libpng_png_filters);
    config.set(section, "ZlibCompressionLevel", zlib_compression_level);
    config.set(section, "ZlibCompressionStrategy", zlib_compression_strategy);
}
void screenshot_statistics::load(const ini_file &config)
{
    static const std::string section = "COUNT";

    ini_data::keys keys;
    config.get(section, keys);

    for (const auto &key : keys)
    {
        if (uint64_t value[2]{}; config.get(section, key, value))
            capture_counts.try_emplace(key, screenshot_statistics_scoped_data{ value[0], value[1] });
    }

    capture_counts.try_emplace({});
}
void screenshot_statistics::save(ini_file &config) const
{
    static const std::string section = "COUNT";

    config.erase(section);

    for (const auto &capture_count : capture_counts)
    {
        uint64_t value[2]{ capture_count.second.total_take, capture_count.second.total_frame };
        config.set(section, capture_count.first, value);
    }
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
    const std::filesystem::path env_all_users_profile = std::wstring_view(buf.as_wchar, size > 0 ? size - 1 : 0);

    // Addon Private Path
    // <= 10.4.2
    // %ALLUSERSPROFILE%/ReShade Addons/Seri/Screenshot
    //  > 10.4.2
    // %ALLUSERSPROFILE%/ReShade Addons/Seri
    addon_private_path = env_all_users_profile / L"ReShade Addons" / L"Seri";
    if (std::filesystem::create_directories(addon_private_path, ec); ec)
    {
        const std::string message = std::format("Failed to create the private path of Add-on with error code %d! '%s' \"%s\"", ec.value(), format_message(ec.value()).c_str(), addon_private_path.u8string().c_str());
        reshade::log::message(reshade::log::level::error, message.c_str());
    }

    // Effect Search Paths
    // <= 10.4.2
    // %ALLUSERSPROFILE%/ReShade Addons/Seri/Screenshot/reshade-shaders/Shaders
    //  > 10.4.2
    // %ALLUSERSPROFILE%/ReShade Addons/Seri/reshade-shaders/Shaders/**
    if (const std::filesystem::path old_screenshotdepth_addonfx = addon_private_path / L"Screenshot" / L"reshade-shaders" / L"Shaders" / L"__Addon_ScreenshotDepth_Seri14.addonfx";
        std::filesystem::status(old_screenshotdepth_addonfx, ec).type() == std::filesystem::file_type::regular)
        std::filesystem::remove(old_screenshotdepth_addonfx, ec);

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

    // Addon Configuration Path
    // <= 10.4.2
    // (DIR:ReShade.DLL)\ReShade_Add-On_Screenshot.ini
    //  > 10.4.2
    // (DIR:ReShade.DLL)\ReShade_Addon_Screenshot.ini
    if (addon_screenshot_config_path = reshade_base_path / L"ReShade_Addon_Screenshot.ini";
        std::filesystem::status(addon_screenshot_config_path, ec).type() == std::filesystem::file_type::not_found)
    {
        const std::filesystem::path old_addon_screenshot_config_path = reshade_base_path / L"ReShade_Add-On_Screenshot.ini";
        if (std::filesystem::status(old_addon_screenshot_config_path, ec).type() == std::filesystem::file_type::regular)
            std::filesystem::rename(old_addon_screenshot_config_path, addon_screenshot_config_path, ec);
    }

    addon_screenshot_statistics_path = reshade_base_path / L"ReShade_Addon_Screenshot.stats";
}
void screenshot_environment::init()
{
    std::error_code ec{};
    const std::filesystem::path addon_shaders = addon_private_path / L"reshade-shaders" / L"Shaders";
    if (std::filesystem::create_directories(addon_shaders, ec); !ec)
    {
        const std::filesystem::path copy_to = addon_shaders / L"__Addon_ScreenshotDepth_Seri14.addonfx";
        if (std::filesystem::path copy_from = addon_shaders / L"__Addon_ScreenshotDepth_Seri14.addonfxgen";
            std::filesystem::status(copy_from).type() == std::filesystem::file_type::regular)
        {
            if (std::filesystem::copy_file(copy_from, copy_to, std::filesystem::copy_options::overwrite_existing, ec); ec)
            {
                const std::string message = std::format("Failed to create the Add-on specific effect with error code %d! '%s' \"%s\"", ec.value(), format_message(ec.value()).c_str(), copy_to.u8string().c_str());
                reshade::log::message(reshade::log::level::error, message.c_str());
            }
        }
        else
        {
            const reshade::resources::data_resource resource = reshade::resources::load_data_resource(IDR_SCREENSHOTDEPTH_ADDONFX);

            enum class condition { none, open, create, blocked };
            auto condition = condition::none;

            const HANDLE file = CreateFileW(copy_to.c_str(), FILE_GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
            ec = std::error_code(GetLastError(), std::system_category());

            if (file == INVALID_HANDLE_VALUE)
                condition = condition::blocked;
            else if (ec.value() == ERROR_ALREADY_EXISTS)
                condition = condition::open;
            else
                condition = condition::create;

            if (condition == condition::open || condition == condition::create)
            {
                if (DWORD _; WriteFile(file, resource.data, static_cast<DWORD>(resource.data_size), &_, NULL) != 0)
                    SetEndOfFile(file);
            }

            if (file != INVALID_HANDLE_VALUE)
            {
                CloseHandle(file);
            }
            else
            {
                const std::string message = std::format("Failed to create the Add-on specific effect with error code %d! '%s' \"%s\"", ec.value(), format_message(ec.value()).c_str(), copy_to.u8string().c_str());
                reshade::log::message(reshade::log::level::error, message.c_str());
            }
        }
    }
    else
    {
        const std::string message = std::format("Failed to create the directory of Add-on specific shaders with error code %d! '%s' \"%s\"", ec.value(), format_message(ec.value()).c_str(), addon_shaders.u8string().c_str());
        reshade::log::message(reshade::log::level::error, message.c_str());
    }

    bool effect_search_paths_updated = false;
    std::vector<std::filesystem::path> effect_search_paths;

    if (size_t buffer_count = 0;
        reshade::get_config_value(nullptr, "GENERAL", "EffectSearchPaths", nullptr, &buffer_count) && buffer_count != 0)
    {
        std::string buf; buf.resize(buffer_count - 1);
        reshade::get_config_value(nullptr, "GENERAL", "EffectSearchPaths", buf.data(), &buffer_count);
        std::wstring wbuf; wbuf.reserve(buf.size());
        utf8::unchecked::utf8to16(buf.cbegin(), buf.cend(), std::back_inserter(wbuf));

        std::filesystem::path *path = &effect_search_paths.emplace_back();
        for (const std::wstring::value_type wc : wbuf)
        {
            if (wc != L'\0')
                *path += wc;
            else
                path = &effect_search_paths.emplace_back();
        }
        if (wbuf.back() == L'\0')
            effect_search_paths.pop_back();
    }

    // Effect Search Paths
    // <= 10.4.2
    // %ALLUSERSPROFILE%/ReShade Addons/Seri/Screenshot/reshade-shaders/Shaders
    //  > 10.4.2
    // %ALLUSERSPROFILE%/ReShade Addons/Seri/reshade-shaders/Shaders/**
    const std::filesystem::path old_effect_search_path = addon_private_path / L"Screenshot" / L"reshade-shaders" / L"Shaders";
    if (auto it = std::remove_if(effect_search_paths.begin(), effect_search_paths.end(),
        [&old_effect_search_path](std::filesystem::path &path) { return path == old_effect_search_path; }); it != effect_search_paths.end())
    {
        effect_search_paths_updated = true;
        effect_search_paths.erase(it, effect_search_paths.end());
    }
    const std::filesystem::path effect_search_path = addon_private_path / L"reshade-shaders" / L"Shaders" / L"**";
    if (std::find_if(effect_search_paths.begin(), effect_search_paths.end(),
        [&effect_search_path](std::filesystem::path &path) { return path == effect_search_path; }) == effect_search_paths.end())
    {
        effect_search_paths_updated = true;
        effect_search_paths.push_back(effect_search_path);
    }

    if (effect_search_paths_updated)
    {
        std::string buf;
        for (std::filesystem::path &path : effect_search_paths)
        {
            buf += path.u8string();
            buf += '\0';
        }
        reshade::set_config_value(nullptr, "GENERAL", "EffectSearchPaths", buf.data(), buf.size());
    }
}

bool screenshot::capture(reshade::api::effect_runtime *const runtime, screenshot_kind kind)
{
    if (runtime == nullptr)
        return false;

    runtime->get_screenshot_width_and_height(&width, &height);
    std::vector<uint32_t> &capture = captures[kind];
    capture.resize(static_cast<size_t>(width) * height);

    switch (kind)
    {
        case screenshot_kind::original:
        case screenshot_kind::before:
        case screenshot_kind::after:
        case screenshot_kind::overlay:
            return runtime->capture_screenshot(capture.data());
        default:
        case screenshot_kind::depth:
            if (runtime->find_technique("__Addon_ScreenshotDepth_Seri14.addonfx", "__Addon_Technique_ScreenshotDepth_Seri14").handle != 0)
            {
                reshade::api::device *const device = runtime->get_device();

                if (device == nullptr)
                    return false;

                auto get_texture_data = [runtime, device](reshade::api::resource resource, reshade::api::resource_usage state, std::vector<uint32_t> &texture_data, reshade::api::format &texture_format) -> bool
                    {
                        const reshade::api::resource_desc desc = device->get_resource_desc(resource);
                        texture_format = reshade::api::format_to_default_typed(desc.texture.format, 0);

                        if (texture_format != reshade::api::format::r32_float)
                        {
                            reshade::log::message(reshade::log::level::error, std::format("Screenshots are not supported for format %u !", desc.texture.format).c_str());
                            return false;
                        }

                        // Copy back buffer data into system memory buffer
                        reshade::api::resource intermediate;
                        if (!device->create_resource(reshade::api::resource_desc(desc.texture.width, desc.texture.height, 1, 1, texture_format, 1, reshade::api::memory_heap::gpu_to_cpu, reshade::api::resource_usage::copy_dest), nullptr, reshade::api::resource_usage::copy_dest, &intermediate))
                        {
                            reshade::log::message(reshade::log::level::error, "Failed to create system memory texture for screenshot capture!");
                            return false;
                        }

                        device->set_resource_name(intermediate, "ReShade screenshot texture");

                        reshade::api::command_list *const cmd_list = runtime->get_command_queue()->get_immediate_command_list();
                        cmd_list->barrier(resource, state, reshade::api::resource_usage::copy_source);
                        cmd_list->copy_texture_region(resource, 0, nullptr, intermediate, 0, nullptr);
                        cmd_list->barrier(resource, reshade::api::resource_usage::copy_source, state);

                        // Wait for any rendering by the application finish before submitting
                        // It may have submitted that to a different queue, so simply wait for all to idle here
                        runtime->get_command_queue()->wait_idle();

                        // Copy data from intermediate image into output buffer
                        reshade::api::subresource_data mapped_data = {};
                        if (device->map_texture_region(intermediate, 0, nullptr, reshade::api::map_access::read_only, &mapped_data))
                        {
                            const uint8_t *mapped_pixels = static_cast<const uint8_t *>(mapped_data.data);
                            const uint32_t pixels_row_pitch = 4u * desc.texture.width;
                            texture_data.resize(desc.texture.width * desc.texture.height);
                            uint8_t *pixels = reinterpret_cast<uint8_t *>(texture_data.data());

                            for (uint32_t y = 0; y < desc.texture.height; ++y, pixels += pixels_row_pitch, mapped_pixels += mapped_data.row_pitch)
                                std::memcpy(pixels, mapped_pixels, pixels_row_pitch);

                            device->unmap_texture_region(intermediate, 0);
                        }

                        device->destroy_resource(intermediate);

                        return mapped_data.data != nullptr;
                    };

                if (reshade::api::effect_texture_variable texture = runtime->find_texture_variable("__Addon_ScreenshotDepth_Seri14.addonfx", "__Addon_Texture_ScreenshotDepth_Seri14"); texture.handle != 0)
                {
                    reshade::api::resource_view rsv{}, rsv_srgb{};
                    if (runtime->get_texture_binding(texture, &rsv, &rsv_srgb); rsv.handle != 0)
                    {
                        if (reshade::api::resource resource = device->get_resource_from_view(rsv); resource.handle != 0)
                        {
                            reshade::api::format texture_format;
                            return get_texture_data(resource, reshade::api::resource_usage::shader_resource, capture, texture_format);
                        }
                    }
                }
            }
            break;
    }

    return false;
}

void screenshot::save_preset(reshade::api::effect_runtime *runtime)
{
    if (runtime == nullptr)
        return;

    std::error_code ec;
    preset_file = expand_macro_string(myset.image_paths[preset].u8string());
    preset_file.replace_extension() += L".ini";
    preset_file = std::filesystem::weakly_canonical(environment.reshade_base_path / preset_file, ec);

    if (std::filesystem::create_directories(preset_file.parent_path(), ec); !ec)
        runtime->export_current_preset(preset_file.u8string().c_str());
}

void screenshot::save_image()
{
    for (size_t i = 0; i < captures.size(); i++)
    {
        if (std::vector<uint32_t> &capture = captures[i]; !capture.empty())
            save_image(static_cast<screenshot_kind>(i));
    }
}
void screenshot::save_image(screenshot_kind kind)
{
    const auto begin = std::chrono::system_clock::now();

    std::error_code ec{};
    enum { ok, open_error, write_error } result = ok;

    const uint64_t freelimit = myset.image_freelimits[kind];

    image_file = myset.image_paths[kind];
    image_file = std::filesystem::u8path(expand_macro_string(image_file.u8string()));
    image_file = std::filesystem::weakly_canonical(environment.reshade_base_path / image_file, ec);

    if (!image_file.has_filename())
    {
        message = std::format("Skip saving '%s' screenshot because the path has no file name! \"%s\"", get_screenshot_kind_name(kind), image_file.u8string().c_str());
        reshade::log::message(reshade::log::level::error, message.c_str());

        result = open_error;

        state.error_occurs++;
        return;
    }

    const std::filesystem::path parent_path = image_file.parent_path();
    if (std::filesystem::create_directories(parent_path, ec), ec)
    {
        message = std::format("Failed to create '%s' screenshot directory with error code %d! '%s' \"%s\"", get_screenshot_kind_name(kind), ec.value(), format_message(ec.value()).c_str(), parent_path.u8string().c_str());
        reshade::log::message(reshade::log::level::error, message.c_str());

        result = open_error;

        state.error_occurs++;
        return;
    }

    if (freelimit != 0)
    {
        if (ULARGE_INTEGER diskBytes{}, freeBytes{};
            GetDiskFreeSpaceExW(parent_path.c_str(), &freeBytes, &diskBytes, nullptr))
        {
            const float free_ratio = static_cast<float>((double)freeBytes.QuadPart / diskBytes.QuadPart);
            bool limit_exceeded = false;

            if (freelimit >= 100) // Limit by absolute value
                limit_exceeded = freeBytes.QuadPart < freelimit * 1024 * 1024 * 1; // FREEBYTES < FREELIMIT(MB)
            else // Limit by percentage value
                limit_exceeded = free_ratio * 100 < freelimit;

            if (limit_exceeded)
            {
                ULARGE_INTEGER usedBytes{}; usedBytes.QuadPart = diskBytes.QuadPart - freeBytes.QuadPart;

                const std::string usedBytesStr = addon::to_size_string(usedBytes.QuadPart);
                const std::string diskBytesStr = addon::to_size_string(diskBytes.QuadPart);

                message = std::format("Blocked to create '%s' screenshot due to disk space limitation! %*s (%.1f%%) used of %*s (%.1f%% free < %llu%%)", get_screenshot_kind_name(kind),
                    usedBytesStr.size(), usedBytesStr.c_str(), (1.0 - free_ratio) * 100,
                    diskBytesStr.size(), diskBytesStr.c_str(), free_ratio * 100, freelimit);
                reshade::log::message(reshade::log::level::error, message.c_str());

                result = open_error;

                state.error_occurs++;
                return;
            }
        }
        else
        {
#ifdef _DEBUG
            ec = std::error_code(GetLastError(), std::system_category());

            const std::string reason = format_message(ec.value());
            const std::string target = parent_path.u8string();

            message = std::format("GetDiskFreeSpaceExW() returns %d! '%*s' \"%*s\"", ec.value(), reason.size(), reason.c_str(), target.size(), target.c_str());
            reshade::log::message(reshade::log::level::debug, message.c_str());
#endif
        }
    }
    if (kind == screenshot_kind::depth)
    {
        int tif_ec = 0;
        image_file.replace_extension() += L".tiff";

        if (TIFF *tif = TIFFOpenW(image_file.c_str(), "wl");
            tif != nullptr)
        {
            TIFFWriteBufferSetup(tif, nullptr, std::min<tmsize_t>(static_cast<size_t>(myset.file_write_buffer_size), sizeof(uint32_t) * captures[screenshot_kind::depth].size()));

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
            uint8_t *buf = reinterpret_cast<uint8_t *>(captures[kind].data());
            for (uint32_t row = 0; row < height; ++row, buf += row_strip_length)
                TIFFWriteScanline(tif, buf, row, static_cast<uint16_t>(row_strip_length));

            TIFFClose(tif);

            enum class condition { none, open, create, blocked };
            auto condition = condition::none;

            const HANDLE meta = CreateFileW(image_file.c_str(), FILE_GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
            ec = std::error_code(GetLastError(), std::system_category());

            if (meta == INVALID_HANDLE_VALUE)
                condition = condition::blocked;
            else if (ec.value() == ERROR_SUCCESS)
                condition = condition::open;

            if (condition == condition::open)
            {
                const uint64_t date_time = std::chrono::duration_cast<std::chrono::nanoseconds>(frame_time.time_since_epoch()).count() / 100 + 116444736000000000;
                FILETIME ft{};
                ft.dwLowDateTime = date_time & 0xFFFFFFFF;
                ft.dwHighDateTime = date_time >> 32;
                SetFileTime(meta, nullptr, nullptr, &ft);
            }

            if (meta != INVALID_HANDLE_VALUE)
            {
                CloseHandle(meta);
            }
            else
            {
                message = std::format("Failed to saving '%s' screenshot with error code %d! '%s' \"%s\"", get_screenshot_kind_name(kind), ec.value(), format_message(ec.value()).c_str(), image_file.u8string().c_str());
                reshade::log::message(reshade::log::level::error, message.c_str());

                result = condition == condition::open ? write_error : open_error;
            }
        }
    }
    else if (myset.image_format == 0 || myset.image_format == 1)
    {
        image_file.replace_extension() += L".png";

        FILE *file = nullptr;
        if (errno_t fopen_error = _wfopen_s(&file, image_file.c_str(), L"wb");
            file != nullptr)
        {
            const unsigned int channels = myset.image_format == 0 ? 3 : 4;
            const unsigned int size = width * height;

            uint8_t *pixel = reinterpret_cast<uint8_t *>(captures[kind].data());
            if (channels == 3)
            {
                for (size_t i = 0; i < size; i++)
                    *((uint32_t *)&pixel[3 * i]) =
                    *((uint32_t *)&pixel[4 * i]);
            }

            png_structp write_ptr = nullptr;
            png_infop info_ptr = nullptr;

            if (write_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, this, user_error_fn, user_warning_fn);
                write_ptr != nullptr)
            {
#pragma warning(disable:4611)
                if (setjmp(*png_set_longjmp_fn(write_ptr, longjmp, sizeof(jmp_buf))) == 0)
                {
#pragma warning(default:4611)
                    setvbuf(file, nullptr, _IOFBF, myset.file_write_buffer_size);

                    png_init_io(write_ptr, file);
                    png_set_filter(write_ptr, PNG_FILTER_TYPE_BASE, myset.libpng_png_filters);

                    png_set_compression_mem_level(write_ptr, MAX_MEM_LEVEL);
                    png_set_compression_buffer_size(write_ptr, 65536);

                    png_set_compression_level(write_ptr, myset.zlib_compression_level);
                    png_set_compression_strategy(write_ptr, myset.zlib_compression_strategy);

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
                else
                {
                    message = std::format("Failed to saving '%s' screenshot! \"%s\"", get_screenshot_kind_name(kind), image_file.u8string().c_str());
                    reshade::log::message(reshade::log::level::error, message.c_str());

                    result = write_error;
                }

                png_destroy_info_struct(write_ptr, &info_ptr);
                png_destroy_write_struct(&write_ptr, &info_ptr);
            }

            fclose(file);

            enum class condition { none, open, create, blocked };
            auto condition = condition::none;

            const HANDLE meta = CreateFileW(image_file.c_str(), FILE_GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
            ec = std::error_code(GetLastError(), std::system_category());

            if (meta == INVALID_HANDLE_VALUE)
                condition = condition::blocked;
            else if (ec.value() == ERROR_SUCCESS)
                condition = condition::open;

            if (condition == condition::open)
            {
                const uint64_t date_time = std::chrono::duration_cast<std::chrono::nanoseconds>(frame_time.time_since_epoch()).count() / 100 + 116444736000000000;
                FILETIME ft{};
                ft.dwLowDateTime = date_time & 0xFFFFFFFF;
                ft.dwHighDateTime = date_time >> 32;
                SetFileTime(meta, nullptr, nullptr, &ft);
            }

            if (meta != INVALID_HANDLE_VALUE)
            {
                CloseHandle(meta);
            }
            else
            {
                message = std::format("Failed to saving '%s' screenshot with error code %d! '%s' \"%s\"", get_screenshot_kind_name(kind), ec.value(), format_message(ec.value()).c_str(), image_file.u8string().c_str());
                reshade::log::message(reshade::log::level::error, message.c_str());

                result = write_error;
            }
        }
        else
        {
            char buffer[BUFSIZ]{};
            if (strerror_s(buffer, fopen_error) == 0)
            {
                message = std::format("Failed to saving '%s' screenshot with error code %d! '%s' \"%s\"", get_screenshot_kind_name(kind), fopen_error, buffer, image_file.u8string().c_str());
                reshade::log::message(reshade::log::level::error, message.c_str());
            }

            result = open_error;
        }
    }
    else if (myset.image_format == 2 || myset.image_format == 3)
    {
        image_file.replace_extension() += L".png";

        const unsigned int channels = myset.image_format == 2 ? 3 : 4;
        const unsigned int size = width * height;

        uint8_t *const pixel = reinterpret_cast<uint8_t *>(captures[kind].data());
        if (channels == 3)
        {
            for (size_t i = 0; i < size; i++)
                *((uint32_t *)&pixel[3 * i]) =
                *((uint32_t *)&pixel[4 * i]);
        }

        if (std::vector<uint8_t> encoded_pixels;
            fpng::fpng_encode_image_to_memory(pixel, width, height, channels, encoded_pixels))
        {
            enum class condition { none, open, create, blocked };
            auto condition = condition::none;

            const HANDLE meta = CreateFileW(image_file.c_str(), FILE_GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_ARCHIVE | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
            ec = std::error_code(GetLastError(), std::system_category());

            if (meta == INVALID_HANDLE_VALUE)
                condition = condition::blocked;
            else if (ec.value() == ERROR_ALREADY_EXISTS)
                condition = condition::open;
            else
                condition = condition::create;

            if (condition == condition::open || condition == condition::create)
            {
                if (DWORD _; WriteFile(meta, encoded_pixels.data(), static_cast<DWORD>(encoded_pixels.size()), &_, NULL) != 0)
                    SetEndOfFile(meta);

                const uint64_t date_time = std::chrono::duration_cast<std::chrono::nanoseconds>(frame_time.time_since_epoch()).count() / 100 + 116444736000000000;
                FILETIME ft{};
                ft.dwLowDateTime = date_time & 0xFFFFFFFF;
                ft.dwHighDateTime = date_time >> 32;
                SetFileTime(meta, nullptr, nullptr, &ft);
            }

            if (meta != INVALID_HANDLE_VALUE)
            {
                CloseHandle(meta);
            }
            else
            {
                message = std::format("Failed to saving '%s' screenshot with error code %d! '%s' \"%s\"", get_screenshot_kind_name(kind), ec.value(), format_message(ec.value()).c_str(), image_file.u8string().c_str());
                reshade::log::message(reshade::log::level::error, message.c_str());

                result = condition == condition::create ? write_error : open_error;
            }
        }
    }
    else if (myset.image_format == 4 || myset.image_format == 5)
    {
        int tif_ec = 0;
        image_file.replace_extension() += L".tiff";

        const unsigned int channels = myset.image_format == 4 ? 3 : 4;
        const unsigned int size = width * height;

        uint8_t *const pixel = reinterpret_cast<uint8_t *>(captures[kind].data());
        if (channels == 3)
        {
            for (size_t i = 0; i < size; i++)
                *((uint32_t *)&pixel[3 * i]) =
                *((uint32_t *)&pixel[4 * i]);
        }

        if (TIFF *tif = TIFFOpenW(image_file.c_str(), "wl");
            tif != nullptr)
        {
            TIFFWriteBufferSetup(tif, nullptr, std::min<tmsize_t>(static_cast<size_t>(myset.file_write_buffer_size), sizeof(uint32_t) * captures[kind].size()));

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
            uint8_t *buf = reinterpret_cast<uint8_t *>(captures[kind].data());
            for (uint32_t row = 0; row < height; ++row, buf += row_strip_length)
                TIFFWriteScanline(tif, buf, row, static_cast<uint16_t>(row_strip_length));

            TIFFClose(tif);

            enum class condition { none, open, create, blocked };
            auto condition = condition::none;

            const HANDLE meta = CreateFileW(image_file.c_str(), FILE_GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
            ec = std::error_code(GetLastError(), std::system_category());

            if (meta == INVALID_HANDLE_VALUE)
                condition = condition::blocked;
            else if (ec.value() == ERROR_SUCCESS)
                condition = condition::open;

            if (condition == condition::open)
            {
                const uint64_t date_time = std::chrono::duration_cast<std::chrono::nanoseconds>(frame_time.time_since_epoch()).count() / 100 + 116444736000000000;
                FILETIME ft{};
                ft.dwLowDateTime = date_time & 0xFFFFFFFF;
                ft.dwHighDateTime = date_time >> 32;
                SetFileTime(meta, nullptr, nullptr, &ft);
            }

            if (meta != INVALID_HANDLE_VALUE)
            {
                CloseHandle(meta);
            }
            else
            {
                message = std::format("Failed to saving '%s' screenshot with error code %d! '%s' \"%s\"", get_screenshot_kind_name(kind), ec.value(), format_message(ec.value()).c_str(), image_file.u8string().c_str());
                reshade::log::message(reshade::log::level::error, message.c_str());

                result = write_error;
            }
        }
        else
        {
            message = std::format("Failed to saving '%s' screenshot path \"%s\"!", get_screenshot_kind_name(kind), image_file.u8string().c_str());
            reshade::log::message(reshade::log::level::error, message.c_str());

            result = open_error;
        }
    }

    if (result == ok)
    {
        const auto elapsed = std::chrono::system_clock::now() - begin;
        state.last_elapsed = elapsed.count();
    }
    else
    {
        state.error_occurs++;

        if (result == write_error)
        {
            if (DeleteFileW(image_file.c_str()) == FALSE)
            {
                ec = std::error_code(GetLastError(), std::system_category());
                reshade::log::message(reshade::log::level::error, std::format("Failed to deleting '%s' screenshot with error code %d! '%s' \"%s\"", get_screenshot_kind_name(kind), ec.value(), format_message(ec.value()).c_str(), image_file.u8string().c_str()).c_str());
            }
        }
    }

    return;
}

std::string screenshot::expand_macro_string(const std::string &input) const
{
    std::list<std::pair<std::string, std::function<std::string(std::string_view)>>> macros;

    macros.emplace_back("APP", [this](std::string_view) { return environment.reshade_executable_path.stem().u8string(); });
    macros.emplace_back("PRESET", [this](std::string_view) { return environment.reshade_preset_path.stem().u8string(); });
    macros.emplace_back("TOTALFRAME",
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
            auto it = statistics.capture_counts.find({});
            return std::format(zeroed ? "%0*" PRIu64 : "%*" PRIu64, digits, it != statistics.capture_counts.end() ? it->second.total_frame : 0);
        });
    macros.emplace_back("MYSETFRAME",
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
            auto it = statistics.capture_counts.find(myset.name);
            return std::format(zeroed ? "%0*" PRIu64 : "%*" PRIu64, digits, it != statistics.capture_counts.end() ? it->second.total_frame : 0);
        });
    macros.emplace_back("TOTALTAKE",
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
            auto it = statistics.capture_counts.find({});
            return std::format(zeroed ? "%0*" PRIu64 : "%*" PRIu64, digits, it != statistics.capture_counts.end() ? it->second.total_take : 0);
        });
    macros.emplace_back("MYSETTAKE",
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
            auto it = statistics.capture_counts.find(myset.name);
            return std::format(zeroed ? "%0*" PRIu64 : "%*" PRIu64, digits, it != statistics.capture_counts.end() ? it->second.total_take : 0);
        });
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

void screenshot::user_error_fn(png_structp png_ptr, png_const_charp error_msg)
{
    if (screenshot *const context = reinterpret_cast<screenshot *>(png_get_error_ptr(png_ptr)); context != nullptr)
    {
        if (const int ec = errno; ec)
        {
            char str[100] = "\0";
            strerror_s(str, ec);

            context->message = std::format("libpng: Fatal error '%s' with error code %d! '%s' \"%s\"", error_msg == nullptr ? "(null)" : *error_msg ? error_msg : "(no message)", ec, str, context->image_file.u8string().c_str());
        }
        else
        {
            context->message = std::format("libpng: Fatal error '%s'! \"%s\"", error_msg == nullptr ? "(null)" : *error_msg ? error_msg : "(no message)", context->image_file.u8string().c_str());
        }
        reshade::log::message(reshade::log::level::error, context->message.c_str());
    }

    png_longjmp(png_ptr, 1);
}
void screenshot::user_warning_fn(png_structp png_ptr, png_const_charp warning_msg)
{
    if (screenshot *const context = reinterpret_cast<screenshot *>(png_get_error_ptr(png_ptr)); context != nullptr)
    {
        if (const int ec = errno; ec)
        {
            char str[100] = "\0";
            strerror_s(str, ec);

            context->message = std::format("libpng: Warning '%s' with error code %d! '%s' \"%s\"", warning_msg == nullptr ? "(null)" : *warning_msg ? warning_msg : "(no message)", ec, str, context->image_file.u8string().c_str());
        }
        else
        {
            context->message = std::format("libpng: Fatal error '%s'! \"%s\"", warning_msg == nullptr ? "(null)" : *warning_msg ? warning_msg : "(no message)", context->image_file.u8string().c_str());
        }
        reshade::log::message(reshade::log::level::warning, context->message.c_str());
    }
}
