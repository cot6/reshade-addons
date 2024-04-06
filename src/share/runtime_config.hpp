/*
 * SPDX-FileCopyrightText: Copyright (C) 2022 Patrick Mours
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "std_string_ext.hpp"

#include <cassert>
#include <charconv>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

inline std::string_view trim(std::string_view str, const char chars[] = " \t") noexcept
{
    if (const size_t found = str.find_last_not_of(chars); found != std::string::npos)
        str = str.substr(0, found + 1);
    if (const size_t found = str.find_first_not_of(chars); found != std::string::npos)
        str = str.substr(found);
    else
        str = str.substr(str.size());
    return str;
}

class ini_data
{
public:
    /// <summary>
    /// Describes a single value in an INI file.
    /// </summary>
    using elements = std::vector<std::string>;
    using sections = std::vector<std::string>;
    /// <summary>
    /// Describes a section of multiple key/value pairs in an INI file.
    /// </summary>
    using table = std::unordered_map<std::string, elements>;
    using entry = std::pair<std::string, elements>;

    ini_data() = default;

    bool empty() const noexcept
    {
        std::lock_guard lock(const_cast<std::recursive_mutex &>(_mutex));

        return _sections.empty();
    }
    bool has(const std::string &section) const noexcept
    {
        std::lock_guard lock(const_cast<std::recursive_mutex &>(_mutex));

        return _sections.find(section) != _sections.end();
    }
    bool has(const std::string &section, const std::string &key) const noexcept
    {
        std::lock_guard lock(const_cast<std::recursive_mutex &>(_mutex));

        const auto it1 = _sections.find(section);
        if (it1 == _sections.end())
            return false;
        const auto it2 = it1->second.find(key);
        if (it2 == it1->second.end())
            return false;
        return true;
    }

    template <typename T>
    bool get(const std::string &section, const std::string &key, T &value) const noexcept
    {
        std::lock_guard lock(const_cast<std::recursive_mutex &>(_mutex));

        const auto it1 = _sections.find(section);
        if (it1 == _sections.end())
            return false;
        const auto it2 = it1->second.find(key);
        if (it2 == it1->second.end())
            return false;
        value = convert<T>(it2->second, 0);
        return true;
    }
    template <typename T, size_t SIZE>
    bool get(const std::string &section, const std::string &key, T(&values)[SIZE]) const noexcept
    {
        std::lock_guard lock(const_cast<std::recursive_mutex &>(_mutex));

        const auto it1 = _sections.find(section);
        if (it1 == _sections.end())
            return false;
        const auto it2 = it1->second.find(key);
        if (it2 == it1->second.end())
            return false;
        for (size_t i = 0; i < SIZE; ++i)
            values[i] = convert<T>(it2->second, i);
        return true;
    }
    template <typename T>
    bool get(const std::string &section, const std::string &key, std::vector<T> &values) const noexcept
    {
        std::lock_guard lock(const_cast<std::recursive_mutex &>(_mutex));

        const auto it1 = _sections.find(section);
        if (it1 == _sections.end())
            return false;
        const auto it2 = it1->second.find(key);
        if (it2 == it1->second.end())
            return false;
        values.resize(it2->second.size());
        for (size_t i = 0; i < it2->second.size(); ++i)
            values[i] = convert<T>(it2->second, i);
        return true;
    }

    void get(sections &sections) const noexcept
    {
        std::lock_guard lock(const_cast<std::recursive_mutex &>(_mutex));

        sections.clear();
        sections.reserve(_sections.size());

        for (const auto &section : _sections)
            sections.push_back(section.first);
    }
    void get(const std::string &section, elements &keys) const noexcept
    {
        std::lock_guard lock(const_cast<std::recursive_mutex &>(_mutex));

        keys.clear();
        const auto it1 = _sections.find(section);
        if (it1 == _sections.end())
            return;

        keys.reserve(it1->second.size());
        for (const auto &it2 : it1->second)
            keys.emplace_back(it2.first);
    }
    void get(const std::string &section, table &table) const noexcept
    {
        std::lock_guard lock(const_cast<std::recursive_mutex &>(_mutex));

        table.clear();
        const auto it1 = _sections.find(section);
        if (it1 == _sections.end())
            return;

        table.reserve(it1->second.size());
        table = it1->second;
    }

    bool set(const std::string &section) noexcept
    {
        std::lock_guard lock(_mutex);

        const bool _ = _sections.try_emplace(section).second;
        if (_)
            _modified_at = std::filesystem::file_time_type::clock::now(),
            _modified = true;
        return _;
    }
    template <typename T>
    void set(const std::string &section, const std::string &key, const T &value) noexcept
    {
        if constexpr (std::is_same<T, float>())
            set(section, key, std::format("%.8e", value));
        else
            set(section, key, std::to_string(value));
    }
    template <>
    void set(const std::string &section, const std::string &key, const bool &value) noexcept
    {
        set<std::string>(section, key, value ? "1" : "0");
    }
    template <>
    void set(const std::string &section, const std::string &key, const std::string &value) noexcept
    {
        std::lock_guard lock(_mutex);

        auto &v = _sections[section][key];
        v.assign(1, value);

        _modified_at = std::filesystem::file_time_type::clock::now();
        _modified = true;
    }
    void set(const std::string &section, const std::string &key, std::string &&value) noexcept
    {
        std::lock_guard lock(_mutex);

        auto &v = _sections[section][key];
        v.resize(1);
        v[0] = std::forward<std::string>(value);

        _modified_at = std::filesystem::file_time_type::clock::now();
        _modified = true;
    }
    template <>
    void set(const std::string &section, const std::string &key, const std::filesystem::path &value) noexcept
    {
        set(section, key, value.u8string());
    }
    template <typename T, size_t SIZE>
    void set(const std::string &section, const std::string &key, const T(&values)[SIZE], const size_t size = SIZE) noexcept
    {
        std::lock_guard lock(_mutex);

        assert(size <= SIZE);

        auto &v = _sections[section][key];
        v.resize(size);
        for (size_t i = 0; i < size; ++i)
            if constexpr (std::is_same<T, float>())
                v[i] = std::format("%.8e", values[i]);
            else
                v[i] = std::to_string(values[i]);

        _modified_at = std::filesystem::file_time_type::clock::now();
        _modified = true;
    }
    void set(const std::string &section, const table &table) noexcept
    {
        std::lock_guard lock(_mutex);

        auto &v = _sections[section];
        v.clear();
        for (const entry &entry : table)
            v[entry.first] = entry.second;

        _modified_at = std::filesystem::file_time_type::clock::now();
        _modified = true;
    }
    template <>
    void set(const std::string &section, const std::string &key, const elements &values) noexcept
    {
        std::lock_guard lock(_mutex);

        auto &v = _sections[section][key];
        v = values;

        _modified_at = std::filesystem::file_time_type::clock::now();
        _modified = true;
    }
    void set(const std::string &section, const std::string &key, elements &&values) noexcept
    {
        std::lock_guard lock(_mutex);

        auto &v = _sections[section][key];
        v = std::forward<elements>(values);

        _modified_at = std::filesystem::file_time_type::clock::now();
        _modified = true;
    }
    template <>
    void set(const std::string &section, const std::string &key, const std::vector<std::filesystem::path> &values) noexcept
    {
        std::lock_guard lock(_mutex);

        auto &v = _sections[section][key];
        v.resize(values.size());
        for (size_t i = 0; i < values.size(); ++i)
            v[i] = values[i].u8string();
        _modified = true;
        _modified_at = std::filesystem::file_time_type::clock::now();
    }

    bool erase(const std::string &section) noexcept
    {
        std::lock_guard lock(_mutex);

        if (_sections.erase(section) == 0)
            return false;

        _modified_at = std::filesystem::file_time_type::clock::now();
        _modified = true;

        return true;
    }
    bool erase(const std::string &section, const std::string &key) noexcept
    {
        std::lock_guard lock(_mutex);

        const auto keys = _sections.find(section);
        if (keys == _sections.end())
            return false;

        if (keys->second.erase(key) == 0)
            return false;

        _modified_at = std::filesystem::file_time_type::clock::now();
        _modified = true;

        return true;
    }

    size_t size(const std::string &section) const noexcept
    {
        std::lock_guard lock(const_cast<std::recursive_mutex &>(_mutex));

        const auto it1 = _sections.find(section);
        if (it1 == _sections.end())
            return 0;
        return it1->second.size();
    }

    template <typename T>
    static T convert(const elements &values, size_t i) noexcept
    {
        T v{};
        return i < values.size() && !values[i].empty() ? std::from_chars(values[i].data(), values[i].data() + values[i].size() + 1, v), v : v;
    }
    template <>
    static bool convert(const elements &values, size_t i) noexcept
    {
        return i < values.size() && !values[i].empty() && (values[i][0] == 't' || values[i][0] == 'T' || convert<long long>(values, i) != 0ll);
    }
    template <>
    static std::string convert(const elements &values, size_t i) noexcept
    {
        return i < values.size() && !values[i].empty() ? values[i] : std::string{};
    }
    template <>
    static std::filesystem::path convert(const elements &values, size_t i) noexcept
    {
        return i < values.size() && !values[i].empty() ? std::filesystem::u8path(values[i]) : std::filesystem::path{};
    }

protected:

    bool _modified = false;
    std::filesystem::file_time_type _modified_at;
    std::unordered_map<std::string, table> _sections;
    std::recursive_mutex _mutex;
};

class ini_file : public ini_data
{
public:
    /// <summary>
    /// Opens the INI file at the specified <paramref name="path"/>.
    /// </summary>
    /// <param name="path">The path to the INI file to access.</param>
    explicit ini_file(const std::filesystem::path &path) noexcept;
    ~ini_file() noexcept;
    bool save() noexcept;

    /// <summary>   
    /// Gets the specified INI file from cache or opens it when it was not cached yet.
    /// WARNING: Reference is only valid until the next 'load_cache' call.
    /// </summary>
    /// <param name="path">The path to the INI file to access.</param>
    /// <returns>A reference to the cached data. This reference is valid until the next call to <see cref="load_cache"/>.</returns>
    static ini_file &load_cache(const std::filesystem::path &path) noexcept;

    static bool flush_cache() noexcept;
    static bool flush_cache(const std::filesystem::path &path) noexcept;

private:
    void load() noexcept;

    std::filesystem::path _path;
    static std::recursive_mutex _static_mutex;
};
