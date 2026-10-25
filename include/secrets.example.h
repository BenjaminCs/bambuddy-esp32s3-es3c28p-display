// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 BenjaminCs - https://github.com/BenjaminCs/bambuddy-esp32s3-es3c28p-display

#pragma once

// Copy this file to include/secrets.h and fill it in.
// secrets.h is in .gitignore – never commit it, it holds your Wi-Fi password and API key.

// 2.4 GHz network only (the ESP32-S3 has no 5 GHz radio).
#define WIFI_SSID        "your-wifi"
#define WIFI_PASSWORD    "your-wifi-password"

// Bambuddy host name, reachable over HTTPS (port 443) from the display's network.
// The certificate must chain to Let's Encrypt ISRG Root X1 or X2 (see src/certs.h);
// for any other CA, replace the PEM in src/certs.h.
#define BAMBUDDY_HOST    "bambuddy.example.com"

// Bambuddy -> Settings -> API keys. Needed permissions:
//   read status, queue, control printer. Everything else can be off.
// Optionally restrict the key to specific printers.
#define BAMBUDDY_API_KEY "bb_..."

// Optional: force a DNS server (comma-separated octets). Use this when your
// Bambuddy host name resolves to the LAN address only on a local DNS (Pi-hole,
// AdGuard, router override) and the DHCP-provided fallback DNS would return the
// public IP. Remove the line to use the DNS servers from DHCP.
// #define DNS_SERVER_IP    192, 168, 1, 2

// Password for over-the-air firmware updates (ArduinoOTA).
#define OTA_PASSWORD     "change-me-to-something-long"
#define DEVICE_HOSTNAME  "bambuddy-display"

// UI language: English by default. Uncomment for Hungarian.
// #define UI_LANGUAGE_HU
