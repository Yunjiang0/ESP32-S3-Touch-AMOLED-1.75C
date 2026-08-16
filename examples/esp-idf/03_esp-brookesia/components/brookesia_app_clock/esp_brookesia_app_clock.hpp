/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

/**
 * @brief Analog clock app for ESP-Brookesia Phone system.
 *        Uses the same visual style as Squareline Demo's clock screen.
 */
class ClockApp: public systems::phone::App {
public:
    static ClockApp *requestInstance(bool use_status_bar = true, bool use_navigation_bar = false);
    ~ClockApp();

    using systems::phone::App::startRecordResource;
    using systems::phone::App::endRecordResource;

protected:
    ClockApp(bool use_status_bar, bool use_navigation_bar);

    bool run(void) override;
    bool back(void) override;
    bool close(void) override;
    bool pause(void) override;
    bool resume(void) override;

private:
    static ClockApp *_instance;

    // UI objects (same structure as Squareline clock screen)
    lv_obj_t *m_screen = nullptr;
    lv_obj_t *m_clock_panel = nullptr;
    lv_obj_t *m_image_hour = nullptr;
    lv_obj_t *m_image_min = nullptr;
    lv_obj_t *m_image_sec = nullptr;
    lv_obj_t *m_label_time = nullptr;
    lv_obj_t *m_label_date = nullptr;
    lv_obj_t *m_center_dot = nullptr;
    lv_timer_t *m_update_timer = nullptr;

    void createClockUI();
    void startAnimations();
    void updateTime();

    static void updateTimerCallback(lv_timer_t *timer);
};

} // namespace esp_brookesia::apps
