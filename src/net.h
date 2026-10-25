// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 BenjaminCs - https://github.com/BenjaminCs/bambuddy-esp32s3-es3c28p-display

#pragma once

#include "model.h"

// Starts Wi-Fi and the network task (FreeRTOS, core 0), so slow HTTPS requests
// never block the display.
void net_begin();

// Copies the current state into `out` if it changed since `last_version`.
// Returns true when a copy was made.
bool net_snapshot(AppState &out, uint32_t last_version);

// Queues a command for the network task (non-blocking; dropped if 4 are pending).
void net_send(const Command &cmd);

// Fetches the next finished command result, if any (non-blocking).
bool net_poll_result(CommandResult &out);
