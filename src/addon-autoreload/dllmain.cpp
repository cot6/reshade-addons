// SPDX-FileCopyrightText: 2018 seri14
// SPDX-License-Identifier: BSD-3-Clause

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <vector>

#include <Windows.h>

#include "dllmain.hpp"
#include "std_string_ext.hpp"

#include <reshade.hpp>
#include <utf8/unchecked.h>

static void on_init_effect_runtime(reshade::api::effect_runtime *runtime)
{
    if (runtime == nullptr)
        return;

    runtime->create_private_data<autoreload_context>();
}
static void on_destroy_effect_runtime(reshade::api::effect_runtime *runtime)
{
    if (runtime == nullptr)
        return;

    runtime->destroy_private_data<autoreload_context>();
}
static void on_reshade_present(reshade::api::effect_runtime *runtime)
{
    if (runtime == nullptr)
        return;

    auto &ctx = runtime->get_private_data<autoreload_context>();

    if (std::addressof(ctx) == nullptr)
        return;

    std::vector<std::string> changes;
    const size_t size = ctx._filesystem_update_listener.read(changes);

    for (const std::string &filename : changes)
        runtime->require_reload_effect(filename.c_str());
}
static void on_reshade_reloaded_effects(reshade::api::effect_runtime *runtime)
{
    if (runtime == nullptr)
        return;

    auto &ctx = runtime->get_private_data<autoreload_context>();

    if (std::addressof(ctx) == nullptr)
        return;

    for (const std::string &directory : ctx._file_watcher.directories())
        ctx._file_watcher.removeWatch(directory);

    std::vector<char> buf; buf.resize(1024 * 64);
    size_t size = 0;

    size = buf.size();
    reshade::get_reshade_base_path(buf.data(), &size);
    assert(size > 0);

    std::filesystem::path reshade_base_path = std::filesystem::u8path(buf.data(), buf.data() + size);

    std::vector<std::filesystem::path> effect_search_paths;
    if (size_t buffer_count = 0;
        reshade::get_config_value(nullptr, "GENERAL", "EffectSearchPaths", nullptr, &buffer_count) && buffer_count != 0)
    {
        std::string cbuf; cbuf.resize(buffer_count - 1);
        reshade::get_config_value(nullptr, "GENERAL", "EffectSearchPaths", cbuf.data(), &buffer_count);
        std::wstring wbuf; wbuf.reserve(cbuf.size());
        utf8::unchecked::utf8to16(cbuf.cbegin(), cbuf.cend(), std::back_inserter(wbuf));

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

        for (std::filesystem::path &path : effect_search_paths)
        {
            std::error_code ec{};
            // First convert path to an absolute path
            // Ignore the working directory and instead start relative paths at the DLL location
            if (path.is_relative())
                path = reshade_base_path / path;
            // Finally try to canonicalize the path too
            if (std::filesystem::path canonical_path = std::filesystem::canonical(path, ec); !ec)
                path = std::move(canonical_path);
            else
                path = path.lexically_normal();
        }
    }

    for (std::filesystem::path &effect_search_path : effect_search_paths)
    {
        const bool recursive = effect_search_path.filename() == L"**";

        if (recursive || !effect_search_path.has_filename())
            effect_search_path = effect_search_path.parent_path();

        const std::string u8path = effect_search_path.u8string();
        const efsw::WatchID watch_id = ctx._file_watcher.addWatch(u8path, reinterpret_cast<efsw::FileWatchListener *>(&ctx._filesystem_update_listener), recursive, { efsw::WatcherOption(efsw::Option::WinNotifyFilter, FILE_NOTIFY_CHANGE_SIZE) });

        if (watch_id <= efsw::Error::NoError)
            reshade::log_message(reshade::log_level::error, efsw::Errors::Log::getLastErrorLog().c_str());
    }

    ctx._file_watcher.watch();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            if (!reshade::register_addon(hModule))
                return FALSE;
            reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
            reshade::register_event<reshade::addon_event::destroy_effect_runtime>(on_destroy_effect_runtime);
            reshade::register_event<reshade::addon_event::reshade_present>(on_reshade_present);
            reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(on_reshade_reloaded_effects);
            break;
        case DLL_PROCESS_DETACH:
            reshade::unregister_addon(hModule);
            break;
    }

    return TRUE;
}
