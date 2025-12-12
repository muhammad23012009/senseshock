/*
 * This file is part of senseshock (https://github.com/muhammad23012009/senseshock)
 * Copyright (c) 2025 Muhammad  <thevancedgamer@mentallysanemainliners.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <map>
#include <string>
#include <cstdint>
#include <locale>
#include <codecvt>

enum Strings : uint8_t {
    Manufacturer = 1,
    Product,
    SerialNumber,
    Config,
    ConfigHs,
    Interface0,
};

std::map<Strings, std::string> string_descriptors = {
    { Manufacturer, "Sony Interactive Entertainment" },
    { Product, "Wireless Controller" },
    { SerialNumber, "ABCDEF0123456789" },
    { Config, "Low speed Configuration" },
    { ConfigHs, "High speed Configuration" },
    { Interface0, "HID Interface" },
};

std::u16string string_to_wstring(const std::string& str)
{
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    return converter.from_bytes(str);
}
