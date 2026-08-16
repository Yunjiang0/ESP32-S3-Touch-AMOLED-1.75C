/*
 * SPDX-FileCopyrightText: 2024 Custom
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <vector>
#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

/**
 * @brief Wallpaper app for ESP-Brookesia Phone system.
 *        Displays images from LittleFS storage as wallpapers with auto-rotation.
 */
class WallpaperApp: public systems::phone::App {
public:
    /**
     * @brief Get the singleton instance of WallpaperApp
     *
     * @param use_status_bar Flag to show the status bar
     * @param use_navigation_bar Flag to show the navigation bar
     * @return Pointer to the singleton instance
     */
    static WallpaperApp *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);

    /**
     * @brief Destructor
     */
    ~WallpaperApp();

    using systems::phone::App::startRecordResource;
    using systems::phone::App::endRecordResource;

protected:
    /**
     * @brief Private constructor to enforce singleton pattern
     */
    WallpaperApp(bool use_status_bar, bool use_navigation_bar);

    /**
     * @brief Called when the app starts running
     */
    bool run(void) override;

    /**
     * @brief Called when the app receives a back event
     */
    bool back(void) override;

    /**
     * @brief Called when the app starts to close
     */
    bool close(void) override;

    /**
     * @brief Called when the app is paused
     */
    bool pause(void) override;

    /**
     * @brief Called when the app resumes
     */
    bool resume(void) override;

private:
    static WallpaperApp *_instance;

    // UI objects
    lv_obj_t *m_root = nullptr;
    lv_obj_t *m_image_obj = nullptr;
    lv_obj_t *m_player_obj = nullptr;   // For GIF
    void *m_player_buf = nullptr;        // GIF memory buffer
    lv_timer_t *m_timer = nullptr;
    lv_obj_t *m_loading_label = nullptr;
    lv_obj_t *m_settings_panel = nullptr;
    lv_timer_t *m_long_press_timer = nullptr;
    lv_obj_t *m_interval_label = nullptr;
    lv_obj_t *m_auto_switch = nullptr;

    // Wallpaper list
    std::vector<std::string> m_wallpapers;
    int m_current_index = 0;

    // Settings
    uint32_t m_interval_ms = 10000;      // Image switch interval
    bool m_auto_play = true;             // Auto slideshow

    // Touch handling
    lv_point_t m_touch_start;
    bool m_touch_active = false;
    uint32_t m_touch_time = 0;
    bool m_long_press_triggered = false;

    // Methods
    void scanWallpapers();
    void showCurrent();
    void showImage(const std::string &path);
    void showGif(const std::string &path);
    void clearPlayer();
    void showNext();
    void showPrev();
    bool isGif(const std::string &path);
    void showLoadingHint();
    void hideLoadingHint();
    void openSettingsPanel();
    void closeSettingsPanel();

    static void timerCallback(lv_timer_t *timer);
    static void touchEventCallback(lv_event_t *e);
    static void longPressTimerCallback(lv_timer_t *timer);
};

} // namespace esp_brookesia::apps
