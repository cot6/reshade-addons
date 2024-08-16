// SPDX-FileCopyrightText: 2018 seri14
// SPDX-License-Identifier: BSD-3-Clause

#include <algorithm>
#include <iterator>
#include <shared_mutex>
#include <vector>

#include "autoreload.hpp"

void filesystem_update_listener::handleFileAction(efsw::WatchID watch_id, const std::string &dir, const std::string &filename, efsw::Action action, std::string old_filename)
{
    const std::shared_lock<std::shared_mutex> lock(_read_mutex);

    if (std::find_if(_changes.cbegin(), _changes.cend(), [&filename](const std::string &item) { return item == filename; }) == _changes.end())
        _changes.emplace_back(filename);
}
size_t filesystem_update_listener::read(std::vector<std::string> &changes)
{
    const std::shared_lock<std::shared_mutex> lock(_read_mutex);
    const size_t size = _changes.size();

    if (size != 0)
    {
        std::move(_changes.begin(), _changes.end(), std::back_inserter(changes));
        _changes.clear();
    }

    return size;
}
