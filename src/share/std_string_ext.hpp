/*
 * SPDX-FileCopyrightText: 2018 seri14
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string>

namespace std
{
	template<class... Args>
	std::string format(string_view fmt, const Args &... args) noexcept {
		std::string s(static_cast<size_t>(::_scprintf(fmt.data(), args...)), '\0');
		return ::snprintf(s.data(), s.size() + 1, fmt.data(), args...), s;
	}
	template<class... Args>
	std::wstring format(wstring_view fmt, const Args &... args) noexcept {
		std::wstring s(static_cast<size_t>(::_scwprintf(fmt.data(), args...)), L'\0');
		return ::swprintf(s.data(), s.size() + 1, fmt.data(), args...), s;
	}
}
