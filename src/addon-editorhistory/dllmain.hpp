/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "res\version.h"

#include <imgui.h>
#include <reshade.hpp>

#include <list>
#include <string>
#include <vector>

#if !defined(_DEBUG) && ADDON_MAJOR < RESHADE_API_VERSION
#error メジャー バージョンはAPI改訂番号と同一である必要があります。
#endif

constexpr size_t HISTORY_LIMIT = 1000;

struct history
{
    enum class kind
    {
        uniform_value = 0,
        technique_state,
        technique_sort,
    };

    union uniform_value
    {
        bool as_bool;
        float as_float[16];
        int32_t as_int[16];
        uint32_t as_uint[16];
    };

    kind kind = kind::uniform_value;

    reshade::api::effect_technique technique_handle = { 0 };
    std::string technique_name;
    bool technique_enabled = false;

    reshade::api::effect_uniform_variable variable_handle = { 0 };
    reshade::api::format variable_basetype = reshade::api::format::unknown;
    uniform_value before = {}, after = {};
    std::vector<reshade::api::effect_technique> sorted, sorting;
    bool confirmed = false;
};

struct __declspec(uuid("2f91f8ec-6f8e-436b-b6cc-d7f8d5f9e44c")) history_context
{
    bool show_overlay = false;
    bool was_updated = false;
    size_t techniques_count = 0;
    size_t history_pos = 0;
    std::list<history> histories;
};
