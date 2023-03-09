/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string>

std::string key_name(unsigned int keycode, bool ctrl, bool shift, bool alt) noexcept;
std::string key_name(unsigned int(&keys)[4]) noexcept;
