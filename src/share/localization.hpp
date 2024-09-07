/*
 * Copyright (C) 2023 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#if RESHADE_LOCALIZATION

#include "dll_resources.hpp"

template <size_t SIZE>
constexpr uint16_t compute_crc16(const char(&message)[SIZE])
{
    uint16_t crc = 0, size = SIZE - 1;
    const char *data = message;
    while (size--)
    {
        crc ^= *data++;
        for (int k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ 0xa001 : crc >> 1;
    }
    return crc;
}

#define _(message) reshade::resources::load_string<compute_crc16(message)>().c_str()
#define __COMPUTE_CRC16(message) compute_crc16(message)

#else

#define _(message) message

#endif
