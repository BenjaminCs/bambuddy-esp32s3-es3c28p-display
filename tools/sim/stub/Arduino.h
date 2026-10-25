// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 BenjaminCs - https://github.com/BenjaminCs/bambuddy-esp32s3-es3c28p-display

// Minimal Arduino.h stand-in so model.h / ui.cpp compile on the host.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
using std::min;
#ifndef HAVE_STRLCPY_STUB
#define HAVE_STRLCPY_STUB
static inline size_t strlcpy(char *d, const char *s, size_t n) {
    size_t l = strlen(s);
    if (n) { size_t c = l >= n ? n - 1 : l; memcpy(d, s, c); d[c] = 0; }
    return l;
}
#endif
