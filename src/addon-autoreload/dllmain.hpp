// SPDX-FileCopyrightText: 2018 seri14
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "res\version.h"
#include "autoreload.hpp"

#include <reshade.hpp>

#if !defined(_DEBUG) && ADDON_MAJOR < RESHADE_API_VERSION
// The major version must be the same as the API revision number.
#endif

class __declspec(uuid("70fbbd42-7153-4a47-b155-c499d68d8d8d")) autoreload_context
{
public:
    autoreload_context() {}
    ~autoreload_context() {}

    filesystem_update_listener _filesystem_update_listener;
    efsw::FileWatcher _file_watcher;
};
