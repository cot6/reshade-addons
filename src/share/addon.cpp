/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "addon.hpp"

std::string addon::to_size_string(const uint64_t size)
{
    if (size >= 1LLU * 1024 * 1024 * 1024 * 1024)
        return std::format("%.3f TB", (double)size / (1LLU * 1024 * 1024 * 1024 * 1024));
    if (size >= 1LLU * 1024 * 1024 * 1024)
        return std::format("%.3f GB", (double)size / (1LLU * 1024 * 1024 * 1024));
    if (size >= 1LLU * 1024 * 1024)
        return std::format("%.3f MB", (double)size / (1LLU * 1024 * 1024));
    if (size >= 1LLU * 1024)
        return std::format("%.3f KB", (double)size / (1LLU * 1024));
    return std::format("%llu byte", size);
};
