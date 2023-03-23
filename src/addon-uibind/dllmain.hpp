// SPDX-FileCopyrightText: 2018 seri14
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#include "res\version.h"

#include <reshade.hpp>

#include <string>

#if !defined(_DEBUG) && ADDON_MAJOR < RESHADE_API_VERSION
#error メジャー バージョンはAPI改訂番号と同一である必要があります。
#endif
