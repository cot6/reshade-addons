/*
 * SPDX-FileCopyrightText: Copyright (C) 2022 Patrick Mours
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "runtime_config.hpp"

#include <execution>
#include <Windows.h>

std::unordered_map<std::filesystem::path, ini_file> g_ini_cache;
std::recursive_mutex ini_file::_static_mutex;

ini_file::ini_file(const std::filesystem::path &path) noexcept
    : _path(path)
{
    load();
}
ini_file::~ini_file() noexcept
{
    save();
}

void ini_file::load() noexcept
{
    std::lock_guard lock(_mutex);

    enum class condition { none, open, not_found, blocked };
    condition condition = condition::none;

    const HANDLE file = CreateFileW(_path.c_str(), FILE_GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);

    if (file == INVALID_HANDLE_VALUE)
        condition = condition::blocked;
    else if (GetLastError() == ERROR_FILE_NOT_FOUND)
        condition = condition::not_found;
    else
        condition = condition::open;

    if (condition == condition::blocked)
        return;

    DWORD file_size = 0;
    std::unique_ptr<char[]> mem;

    if (condition == condition::open)
    {
        using filetime_duration = std::chrono::duration<uint64_t, std::ratio<100, 1000000000>>; // 100 nanoseconds
        using filetime_time_point = std::chrono::time_point<std::filesystem::_File_time_clock, filetime_duration>;

        FILETIME last_write_time{};
        GetFileTime(file, NULL, NULL, &last_write_time);

        if (const std::filesystem::file_time_type modified_at = filetime_time_point(filetime_duration((uint64_t &)last_write_time));
            _modified_at < modified_at)
        {
            _modified_at = modified_at;

            // Reservating memory (limit to 150KB)
            file_size = std::min<DWORD>(150 * 1024, GetFileSize(file, NULL));

            // Read file contents into memory
            if (mem = std::make_unique<char[]>(file_size); ReadFile(file, mem.get(), file_size, &file_size, NULL) == 0)
                mem = {};
        }
    }

    // No longer need to have a handle open to the file, since all data was read, so can safely close it
    CloseHandle(file);

    if (!mem)
        return;

    _modified = false;
    _sections = {};

    // Remove BOM (0xefbbbf means 0xfeff)
    if (file_size >= 3 &&
        mem[0] == 0xef &&
        mem[1] == 0xbb &&
        mem[2] == 0xbf)
        mem[0] = '\n',
        mem[1] = '\n',
        mem[2] = '\n';

    // Create read content view
    std::string_view data(mem.get(), file_size);

    ini_data::table *section = nullptr;
    for (size_t next = 0; next = std::min(std::min(data.find_first_of('\n'), data.size()), data.size()), !data.empty(); data = data.substr(std::min(next + 1, data.size())))
    {
        const std::string_view line = trim({ data.data(), next }, " \t\r");

        if (line.empty() || line.front() == ';' || line.front() == '/')
            continue;

        if (line.front() == '[')
        {
            if (line.back() == ']')
                section = &_sections[std::string(trim(line.substr(1, line.size() - 2)))];

            continue;
        }

        const size_t assign_index = line.find_first_of('=');
        if (assign_index == std::string::npos)
            continue;

        std::string key(trim(line.substr(0, assign_index)));
        std::string_view value(trim(line.substr(assign_index + 1)));

        if (section == nullptr)
            section = &_sections[{}];

        if (ini_data::elements &elements = (*section)[key]; elements.empty())
        {
            for (size_t offset = 0, found = 0; found = std::min(std::min(value.find_first_of(',', offset), value.size()), value.size()), !value.empty();)
            {
                if (offset = 0; found + 2 < value.size() && value[found + 1] == ',')
                {
                    offset = found + 2;
                    continue;
                }
                std::string &element = elements.emplace_back(); element.reserve(found);
                for (const char c : trim(value.substr(0, found)))
                    if (element.empty() || c != ',' || element.back() != ',')
                        element += c;
                value = value.substr(std::min(found + 1, value.size()));
            }
        }
    }
}
bool ini_file::save() noexcept
{
    std::lock_guard lock(_mutex);

    if (!_modified)
        return true;

    enum class condition { none, open, create, blocked };
    auto condition = condition::none;

    const HANDLE file = CreateFileW(_path.c_str(), FILE_GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_ARCHIVE | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

    if (file == INVALID_HANDLE_VALUE)
        condition = condition::blocked;
    else if (GetLastError() == ERROR_ALREADY_EXISTS)
        condition = condition::open;
    else
        condition = condition::create;

    if (condition == condition::blocked)
        return false;

    std::string str; str.reserve(20 * 1024);
    std::vector<std::string> section_names, key_names;

    section_names.reserve(_sections.size());
    for (const auto &section : _sections)
        section_names.push_back(section.first);

    // Sort sections to generate consistent files
    std::sort(std::execution::seq, section_names.begin(), section_names.end(), [](std::string a, std::string b) noexcept {
        std::transform(std::execution::seq, a.cbegin(), a.cend(), a.begin(), [](const char c) noexcept { return ('a' <= c && c <= 'z') ? static_cast<char>(c - ' ') : c; });
        std::transform(std::execution::seq, b.cbegin(), b.cend(), b.begin(), [](const char c) noexcept { return ('a' <= c && c <= 'z') ? static_cast<char>(c - ' ') : c; });
        return a < b;
        });

    for (const std::string &section_name : section_names)
    {
        const auto &keys = _sections.at(section_name);

        key_names.reserve(keys.size());
        for (const auto &key : keys)
            key_names.push_back(key.first);

        std::sort(std::execution::seq, key_names.begin(), key_names.end(), [](std::string a, std::string b) noexcept {
            std::transform(std::execution::seq, a.cbegin(), a.cend(), a.begin(), [](const char c) noexcept { return ('a' <= c && c <= 'z') ? static_cast<char>(c - ' ') : c; });
            std::transform(std::execution::seq, b.cbegin(), b.cend(), b.begin(), [](const char c) noexcept { return ('a' <= c && c <= 'z') ? static_cast<char>(c - ' ') : c; });
            return a < b;
            });

        // Empty section should have been sorted to the top, so do not need to append it before keys
        if (!section_name.empty())
            str.append(1, '[').append(section_name).append(1, ']').append(1, '\n');

        for (const std::string &key_name : key_names)
        {
            str.append(key_name).append(1, '=');
            if (const auto &elements = keys.at(key_name); !elements.empty())
            {
                for (const std::string &element : elements)
                {
                    for (const char c : element)
                        str.append(c == ',' ? 2 : 1, c);
                    str += ','; // Separate multiple values with a comma
                }
                str.back() = '\n';
                continue;
            }
            str.append(1, '\n');
        }

        str.append(1, '\n');
        key_names.clear();
    }

    if (DWORD _; WriteFile(file, str.data(), static_cast<DWORD>(str.size()), &_, NULL) != 0)
        SetEndOfFile(file), _modified = false;

    const uint64_t date_time = (std::chrono::duration_cast<std::chrono::nanoseconds>(_modified_at.time_since_epoch()).count() + 11644473600000000000) / 100;
    FILETIME ft{};
    ft.dwLowDateTime = date_time & 0xFFFFFFFF;
    ft.dwHighDateTime = date_time >> 32;
    SetFileTime(file, nullptr, nullptr, &ft);

    CloseHandle(file);

    return true;
}

ini_file &ini_file::load_cache(const std::filesystem::path &path) noexcept
{
    std::lock_guard lock(_static_mutex);

    const auto it = g_ini_cache.try_emplace(path, path);
    if (it.second || (std::filesystem::file_time_type::clock::now() - it.first->second._modified_at) < std::chrono::seconds(1))
        return it.first->second; // Don't need to reload file when it was just loaded or there are still modifications pending
    else
        return it.first->second.load(), it.first->second;
}

bool ini_file::flush_cache() noexcept
{
    std::unique_lock lock(_static_mutex, std::try_to_lock);

    if (!lock.owns_lock())
        return false;

    const std::filesystem::file_time_type now = std::filesystem::file_time_type::clock::now();

    // Save all files that were modified in one second intervals
    for (auto &file : g_ini_cache)
        if (file.second._modified && (now - file.second._modified_at) > std::chrono::seconds(1))
            file.second.save();

    return true;
}
bool ini_file::flush_cache(const std::filesystem::path &path) noexcept
{
    std::lock_guard lock(_static_mutex);

    if (const auto it = g_ini_cache.find(path); it != g_ini_cache.end())
        return it->second.save();
    return false;
}
