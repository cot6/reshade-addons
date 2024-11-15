/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <utility>
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
static void on_present(reshade::api::command_queue *, reshade::api::swapchain *swapchain, const reshade::api::rect *, const reshade::api::rect *, uint32_t, const reshade::api::rect *)
{
    reshade::api::effect_runtime *runtime = nullptr;
    swapchain->get_private_data(s_runtime_id, reinterpret_cast<uint64_t *>(&runtime));
    if (runtime == nullptr)
        return;
    autoreload_context &ctx = runtime->get_private_data<autoreload_context>();
    if (std::addressof(ctx) == nullptr)
        return;

    ctx._current_frame++;
}
static void on_reshade_present(reshade::api::effect_runtime *runtime)
{
    if (runtime == nullptr)
        return;

    auto &ctx = runtime->get_private_data<autoreload_context>();

    if (std::addressof(ctx) == nullptr)
        return;

    if (ctx._current_frame == ctx._reloading_frame + 1)
    {
        if (!ctx._reloading_techniques.empty())
        {
            std::vector<std::pair<reshade::api::effect_technique, std::string>> techniques_order;
            runtime->enumerate_techniques(nullptr,
                [&techniques_order](reshade::api::effect_runtime *runtime, reshade::api::effect_technique technique) {
                    char technique_name[256]{};
                    size_t size = sizeof(technique_name);
                    runtime->get_technique_name(technique, technique_name, &size);
                    techniques_order.emplace_back(technique, std::string(technique_name, size));
                });

            for (technique_state &state : ctx._reloading_techniques)
            {
                std::vector<reshade::api::effect_technique> order(techniques_order.size(), reshade::api::effect_technique{ std::numeric_limits<uint64_t>::max() });

                runtime->enumerate_techniques(nullptr,
                    [&ctx, &techniques_order, &order](reshade::api::effect_runtime *runtime, reshade::api::effect_technique technique) {
                        auto order_it = std::find_if(techniques_order.cbegin(), techniques_order.cend(), [technique](const std::pair<reshade::api::effect_technique, std::string> &pair) { return pair.first == technique; });
                        if (order_it == techniques_order.cend())
                            return;
                        const std::string &name = order_it->second;

                        if (auto state_it = std::find_if(ctx._reloading_techniques.begin(), ctx._reloading_techniques.end(), [&name](technique_state &state) { return state.name == name; });
                            state_it != ctx._reloading_techniques.end())
                        {
                            technique_state &state = *state_it;
                            state.used = true;

                            if (state.enabled && !runtime->get_technique_state(technique))
                                runtime->set_technique_state(technique, true);

                            if (techniques_order.size() == state.order.size())
                            {
                                std::fill(order.begin(), order.end(), reshade::api::effect_technique{ std::numeric_limits<uint64_t>::max() });
                                for (size_t i = 0; order.size() > i; i++)
                                {
                                    const std::string &name = state.order[i];
                                    auto it = std::find_if(techniques_order.cbegin(), techniques_order.cend(), [&name](const std::pair<reshade::api::effect_technique, std::string> &pair) {
                                        return name == pair.second; });
                                    if (it != techniques_order.cend())
                                        order[i] = it->first;
                                    else
                                        break;
                                }
                            }
                        }
                    });

                if (std::find(order.cbegin(), order.cend(), reshade::api::effect_technique{ std::numeric_limits<uint64_t>::max() }) == order.cend())
                    runtime->reorder_techniques(order.size(), order.data());
            }

            if (auto remove_it = std::find_if(ctx._reloading_techniques.begin(), ctx._reloading_techniques.end(), [](technique_state &state) { return state.used; });
                remove_it != ctx._reloading_techniques.end())
                ctx._reloading_techniques.erase(remove_it, ctx._reloading_techniques.end());
        }
        if (ctx._reloading_techniques.empty())
            ctx._reloaded_frame = ctx._current_frame;
    }

    if (std::vector<std::string> changes;
        ctx._filesystem_update_listener.read(changes), !changes.empty())
    {
        for (const std::string &filename : changes)
        {
            runtime->enumerate_techniques(filename.c_str(),
                [&ctx](reshade::api::effect_runtime *runtime, reshade::api::effect_technique technique) {
                    char technique_name[256]{};
                    size_t size = sizeof(technique_name);
                    runtime->get_technique_name(technique, technique_name, &size);
                    const std::string_view name(technique_name, size);
                    auto it = std::find_if(ctx._reloading_techniques.begin(), ctx._reloading_techniques.end(), [&name](technique_state &state) {return state.name == name; });
                    technique_state &state = it == ctx._reloading_techniques.end() ? ctx._reloading_techniques.emplace_back() : *it;
                    if (state.name.empty())
                        state.name = name;
                    if (state.enabled = runtime->get_technique_state(technique); state.enabled)
                        state.order.clear();
                    if (state.order.empty())
                    {
                        runtime->enumerate_techniques(nullptr,
                           [&state](reshade::api::effect_runtime *runtime, reshade::api::effect_technique technique) {
                               char technique_name[256]{};
                               size_t size = sizeof(technique_name);
                               runtime->get_technique_name(technique, technique_name, &size);
                               state.order.emplace_back(technique_name, size);
                           });
                    }
                });

            runtime->reload_effect_next_frame(filename.c_str());
        }
    }
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

    std::vector<char> buf; buf.resize(static_cast<size_t>(1024) * 64);
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
        ctx._file_watcher.addWatch(u8path, reinterpret_cast<efsw::FileWatchListener *>(&ctx._filesystem_update_listener), recursive, { efsw::WatcherOption(efsw::Option::WinNotifyFilter, FILE_NOTIFY_CHANGE_SIZE) });
    }

    ctx._file_watcher.watch();
    ctx._reloading_frame = ctx._current_frame;
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
            reshade::register_event<reshade::addon_event::present>(on_present);
            reshade::register_event<reshade::addon_event::reshade_present>(on_reshade_present);
            reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(on_reshade_reloaded_effects);
            break;
        case DLL_PROCESS_DETACH:
            reshade::unregister_addon(hModule);
            break;
    }

    return TRUE;
}
