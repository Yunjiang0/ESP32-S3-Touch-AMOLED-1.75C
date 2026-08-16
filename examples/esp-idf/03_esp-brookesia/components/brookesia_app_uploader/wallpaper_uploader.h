/*
 * SPDX-FileCopyrightText: 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * WiFi AP + HTTP server for uploading wallpapers via browser.
 * User connects to ESP32-S3 hotspot, opens 192.168.4.1, uploads files.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *ap_ssid;          // default "Wallpaper-AP"
    const char *ap_password;      // NULL = open, otherwise >=8 chars
    uint8_t channel;              // 1-13
    uint8_t max_connections;      // default 4
    const char *storage_root;     // default "/storage" (LittleFS mount point)
} wallpaper_uploader_config_t;

#define WALLPAPER_UPLOADER_DEFAULT_CONFIG() { \
    .ap_ssid = "Wallpaper-AP", \
    .ap_password = "12345678", \
    .channel = 1, \
    .max_connections = 4, \
    .storage_root = "/storage", \
}

/**
 * Start AP mode and HTTP server. Returns true on success.
 * Must be called after WiFi is initialized (esp_wifi_start()) and
 * the storage filesystem is mounted.
 */
bool wallpaper_uploader_start(const wallpaper_uploader_config_t *config);

/**
 * Stop AP and HTTP server, free resources.
 */
void wallpaper_uploader_stop(void);

/**
 * Check if any client is currently uploading.
 */
bool wallpaper_uploader_is_busy(void);

/**
 * Get count of stored wallpaper files.
 */
size_t wallpaper_uploader_get_file_count(void);

#ifdef __cplusplus
}
#endif