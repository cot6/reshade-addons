// SPDX-FileCopyrightText: 2018 seri14
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <efsw/efsw.hpp>

class filesystem_update_listener : efsw::FileWatchListener
{
public:
    void handleFileAction(efsw::WatchID watch_id, const std::string &dir, const std::string &filename, efsw::Action action, const std::string &old_filename) override;
    size_t read(std::vector<std::string> &changes);
private:
    std::mutex _read_mutex;
    std::vector<std::string> _changes;
};
