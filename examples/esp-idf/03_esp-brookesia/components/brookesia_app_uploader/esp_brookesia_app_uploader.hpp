/*
 * SPDX-FileCopyrightText: 2024 Custom
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

/**
 * @brief WiFi Uploader app - starts WiFi AP for uploading wallpapers via browser
 */
class UploaderApp: public systems::phone::App {
public:
    static UploaderApp *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);
    ~UploaderApp();

    using systems::phone::App::startRecordResource;
    using systems::phone::App::endRecordResource;

protected:
    UploaderApp(bool use_status_bar, bool use_navigation_bar);
    bool run(void) override;
    bool back(void) override;
    bool close(void) override;
    bool pause(void) override;
    bool resume(void) override;

private:
    static UploaderApp *_instance;

    lv_obj_t *m_root = nullptr;
    lv_obj_t *m_status_label = nullptr;
    lv_obj_t *m_info_label = nullptr;
    lv_obj_t *m_count_label = nullptr;
    lv_obj_t *m_start_btn = nullptr;
    lv_obj_t *m_stop_btn = nullptr;
    lv_timer_t *m_refresh_timer = nullptr;

    bool m_wifi_started = false;

    void startWiFi();
    void stopWiFi();
    void updateUI();
    
    static void refreshTimerCallback(lv_timer_t *timer);
};

} // namespace esp_brookesia::apps
