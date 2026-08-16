/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <ctime>
#include "lvgl.h"
#include "esp_brookesia.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Clock"
#include "esp_lib_utils.h"
#include "esp_brookesia_app_clock.hpp"

#define APP_NAME "Clock"

using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems;

// Declare images (copied from Squareline)
LV_IMG_DECLARE(ui_img_pattern_png);
LV_IMG_DECLARE(ui_img_clock_hour_png);
LV_IMG_DECLARE(ui_img_clock_min_png);
LV_IMG_DECLARE(ui_img_clock_sec_png);

// Declare fonts
LV_FONT_DECLARE(ui_font_Number);

// Declare app icon
LV_IMG_DECLARE(esp_brookesia_app_icon_launcher_clock_112_112);

namespace esp_brookesia::apps {

// Animation helper structures (same as Squareline)
typedef struct {
    lv_obj_t *target;
    int32_t val;
} ui_anim_user_data_t;

static void _ui_anim_callback_free_user_data(lv_anim_t *a)
{
    lv_free(a->user_data);
    a->user_data = nullptr;
}

static void _ui_anim_callback_set_opacity(lv_anim_t *a, int32_t v)
{
    ui_anim_user_data_t *data = (ui_anim_user_data_t *)a->user_data;
    lv_obj_set_style_opa(data->target, v, LV_PART_MAIN);
}

static void _ui_anim_callback_set_image_angle(lv_anim_t *a, int32_t v)
{
    ui_anim_user_data_t *data = (ui_anim_user_data_t *)a->user_data;
    lv_image_set_rotation(data->target, v);
}

static void _ui_anim_callback_set_y(lv_anim_t *a, int32_t v)
{
    ui_anim_user_data_t *data = (ui_anim_user_data_t *)a->user_data;
    lv_obj_set_y(data->target, v);
}

ClockApp *ClockApp::_instance = nullptr;

ClockApp *ClockApp::requestInstance(bool use_status_bar, bool use_navigation_bar)
{
    if (_instance == nullptr) {
        _instance = new ClockApp(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

ClockApp::ClockApp(bool use_status_bar, bool use_navigation_bar):
    App(APP_NAME, &esp_brookesia_app_icon_launcher_clock_112_112, false, use_status_bar, use_navigation_bar)
{
}

ClockApp::~ClockApp()
{
}

bool ClockApp::run(void)
{
    ESP_UTILS_LOGD("Run");

    createClockUI();
    startAnimations();
    updateTime();

    // Create timer to update time every second
    m_update_timer = lv_timer_create(updateTimerCallback, 1000, this);

    return true;
}

bool ClockApp::back(void)
{
    ESP_UTILS_LOGD("Back");
    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
    return true;
}

bool ClockApp::close(void)
{
    ESP_UTILS_LOGD("Close");
    if (m_update_timer) {
        lv_timer_delete(m_update_timer);
        m_update_timer = nullptr;
    }
    return true;
}

bool ClockApp::pause(void)
{
    ESP_UTILS_LOGD("Pause");
    if (m_update_timer) {
        lv_timer_pause(m_update_timer);
    }
    return true;
}

bool ClockApp::resume(void)
{
    ESP_UTILS_LOGD("Resume");
    if (m_update_timer) {
        lv_timer_resume(m_update_timer);
        lv_timer_reset(m_update_timer);
    }
    updateTime();
    return true;
}

void ClockApp::createClockUI()
{
    // Main screen with pattern background (same as Squareline ui_screen_clock)
    m_screen = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(m_screen);
    lv_obj_set_size(m_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(m_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_image_src(m_screen, &ui_img_pattern_png, LV_PART_MAIN);
    lv_obj_set_style_bg_image_tiled(m_screen, true, LV_PART_MAIN);

    // Clock panel (circular container)
    m_clock_panel = lv_obj_create(m_screen);
    lv_obj_set_width(m_clock_panel, 180);
    lv_obj_set_height(m_clock_panel, 180);
    lv_obj_set_x(m_clock_panel, 0);
    lv_obj_set_y(m_clock_panel, 40);
    lv_obj_set_align(m_clock_panel, LV_ALIGN_CENTER);
    lv_obj_remove_flag(m_clock_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(m_clock_panel, 500, LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_clock_panel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_clock_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(m_clock_panel, 0, LV_PART_MAIN);

    // Clock dots (hour markers)
    const int dot_positions[][3] = {
        {-40, 15, LV_ALIGN_TOP_RIGHT},
        {-10, 50, LV_ALIGN_DEFAULT},
        {40, 15, LV_ALIGN_TOP_LEFT},
        {10, 50, LV_ALIGN_TOP_LEFT},
        {10, -50, LV_ALIGN_BOTTOM_LEFT},
        {40, -15, LV_ALIGN_BOTTOM_LEFT},
        {-10, -50, LV_ALIGN_BOTTOM_RIGHT},
        {-40, -15, LV_ALIGN_BOTTOM_RIGHT},
    };

    for (int i = 0; i < 8; i++) {
        lv_obj_t *dot = lv_obj_create(m_clock_panel);
        lv_obj_set_width(dot, 6);
        lv_obj_set_height(dot, 6);
        lv_obj_set_x(dot, dot_positions[i][0]);
        lv_obj_set_y(dot, dot_positions[i][1]);
        lv_obj_set_align(dot, (lv_align_t)dot_positions[i][2]);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(dot, 50, LV_PART_MAIN);
        lv_obj_set_style_bg_color(dot, lv_color_hex(0xB3B4E5), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, 255, LV_PART_MAIN);
        lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
    }

    // Clock number labels (12, 3, 6, 9)
    const char *numbers[] = {"12", "3", "6", "9"};
    const lv_align_t aligns[] = {LV_ALIGN_TOP_MID, LV_ALIGN_RIGHT_MID, LV_ALIGN_BOTTOM_MID, LV_ALIGN_LEFT_MID};
    
    for (int i = 0; i < 4; i++) {
        lv_obj_t *label = lv_label_create(m_clock_panel);
        lv_obj_set_width(label, LV_SIZE_CONTENT);
        lv_obj_set_height(label, LV_SIZE_CONTENT);
        lv_obj_set_align(label, aligns[i]);
        lv_label_set_text(label, numbers[i]);
        lv_obj_set_style_text_color(label, lv_color_hex(0x000746), LV_PART_MAIN);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_18, LV_PART_MAIN);
    }

    // Minute hand
    m_image_min = lv_image_create(m_clock_panel);
    lv_image_set_src(m_image_min, &ui_img_clock_min_png);
    lv_obj_set_width(m_image_min, LV_SIZE_CONTENT);
    lv_obj_set_height(m_image_min, LV_SIZE_CONTENT);
    lv_obj_set_align(m_image_min, LV_ALIGN_CENTER);
    lv_obj_add_flag(m_image_min, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_remove_flag(m_image_min, LV_OBJ_FLAG_SCROLLABLE);
    lv_image_set_pivot(m_image_min, 6, 54);  // Set pivot point for rotation

    // Hour hand
    m_image_hour = lv_image_create(m_clock_panel);
    lv_image_set_src(m_image_hour, &ui_img_clock_hour_png);
    lv_obj_set_width(m_image_hour, LV_SIZE_CONTENT);
    lv_obj_set_height(m_image_hour, LV_SIZE_CONTENT);
    lv_obj_set_align(m_image_hour, LV_ALIGN_CENTER);
    lv_obj_add_flag(m_image_hour, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_remove_flag(m_image_hour, LV_OBJ_FLAG_SCROLLABLE);
    lv_image_set_pivot(m_image_hour, 6, 40);  // Set pivot point for rotation

    // Second hand
    m_image_sec = lv_image_create(m_clock_panel);
    lv_image_set_src(m_image_sec, &ui_img_clock_sec_png);
    lv_obj_set_width(m_image_sec, LV_SIZE_CONTENT);
    lv_obj_set_height(m_image_sec, LV_SIZE_CONTENT);
    lv_obj_set_align(m_image_sec, LV_ALIGN_CENTER);
    lv_obj_add_flag(m_image_sec, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_remove_flag(m_image_sec, LV_OBJ_FLAG_SCROLLABLE);
    lv_image_set_pivot(m_image_sec, 3, 64);  // Set pivot point for rotation

    // Center dot
    m_center_dot = lv_obj_create(m_clock_panel);
    lv_obj_set_width(m_center_dot, 8);
    lv_obj_set_height(m_center_dot, 8);
    lv_obj_set_align(m_center_dot, LV_ALIGN_CENTER);
    lv_obj_remove_flag(m_center_dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(m_center_dot, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_center_dot, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_center_dot, 255, LV_PART_MAIN);
    lv_obj_set_style_border_color(m_center_dot, lv_color_hex(0x1937D2), LV_PART_MAIN);
    lv_obj_set_style_border_opa(m_center_dot, 255, LV_PART_MAIN);
    lv_obj_set_style_border_width(m_center_dot, 2, LV_PART_MAIN);

    // Digital time display
    m_label_time = lv_label_create(m_screen);
    lv_obj_set_width(m_label_time, LV_SIZE_CONTENT);
    lv_obj_set_height(m_label_time, LV_SIZE_CONTENT);
    lv_obj_set_x(m_label_time, 0);
    lv_obj_set_y(m_label_time, 15);
    lv_obj_set_align(m_label_time, LV_ALIGN_TOP_MID);
    lv_label_set_text(m_label_time, "00:00");
    lv_obj_set_style_text_color(m_label_time, lv_color_hex(0x293062), LV_PART_MAIN);
    lv_obj_set_style_text_font(m_label_time, &ui_font_Number, LV_PART_MAIN);

    // Date display
    m_label_date = lv_label_create(m_screen);
    lv_obj_set_width(m_label_date, LV_SIZE_CONTENT);
    lv_obj_set_height(m_label_date, LV_SIZE_CONTENT);
    lv_obj_set_x(m_label_date, 0);
    lv_obj_set_y(m_label_date, 76);
    lv_obj_set_align(m_label_date, LV_ALIGN_TOP_MID);
    lv_label_set_text(m_label_date, "Mon 1 Jan");
    lv_obj_set_style_text_color(m_label_date, lv_color_hex(0x9C9CD9), LV_PART_MAIN);
    lv_obj_set_style_text_font(m_label_date, &lv_font_montserrat_18, LV_PART_MAIN);
}

void ClockApp::startAnimations()
{
    // Panel slide up animation
    {
        ui_anim_user_data_t *data = (ui_anim_user_data_t *)lv_malloc(sizeof(ui_anim_user_data_t));
        data->target = m_clock_panel;
        data->val = -1;
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_time(&anim, 200);
        lv_anim_set_user_data(&anim, data);
        lv_anim_set_custom_exec_cb(&anim, _ui_anim_callback_set_y);
        lv_anim_set_values(&anim, 70, 40);  // Slide from 70 to 40
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        lv_anim_set_delay(&anim, 100);
        lv_anim_set_deleted_cb(&anim, _ui_anim_callback_free_user_data);

        ESP_UTILS_CHECK_FALSE_EXIT(startRecordResource(), "Start record resource failed");
        lv_anim_start(&anim);
        ESP_UTILS_CHECK_FALSE_EXIT(endRecordResource(), "End record resource failed");
    }

    // Hour hand rotation animation
    {
        ui_anim_user_data_t *data = (ui_anim_user_data_t *)lv_malloc(sizeof(ui_anim_user_data_t));
        data->target = m_image_hour;
        data->val = -1;
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_time(&anim, 1000);
        lv_anim_set_user_data(&anim, data);
        lv_anim_set_custom_exec_cb(&anim, _ui_anim_callback_set_image_angle);
        lv_anim_set_values(&anim, 0, 2800);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        lv_anim_set_delay(&anim, 200);
        lv_anim_set_deleted_cb(&anim, _ui_anim_callback_free_user_data);

        ESP_UTILS_CHECK_FALSE_EXIT(startRecordResource(), "Start record resource failed");
        lv_anim_start(&anim);
        ESP_UTILS_CHECK_FALSE_EXIT(endRecordResource(), "End record resource failed");
    }

    // Minute hand rotation animation
    {
        ui_anim_user_data_t *data = (ui_anim_user_data_t *)lv_malloc(sizeof(ui_anim_user_data_t));
        data->target = m_image_min;
        data->val = -1;
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_time(&anim, 1000);
        lv_anim_set_user_data(&anim, data);
        lv_anim_set_custom_exec_cb(&anim, _ui_anim_callback_set_image_angle);
        lv_anim_set_values(&anim, 0, 2100);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        lv_anim_set_delay(&anim, 400);
        lv_anim_set_deleted_cb(&anim, _ui_anim_callback_free_user_data);

        ESP_UTILS_CHECK_FALSE_EXIT(startRecordResource(), "Start record resource failed");
        lv_anim_start(&anim);
        ESP_UTILS_CHECK_FALSE_EXIT(endRecordResource(), "End record resource failed");
    }

    // Second hand continuous rotation
    {
        ui_anim_user_data_t *data = (ui_anim_user_data_t *)lv_malloc(sizeof(ui_anim_user_data_t));
        data->target = m_image_sec;
        data->val = -1;
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_time(&anim, 60000);  // 60 seconds for full rotation
        lv_anim_set_user_data(&anim, data);
        lv_anim_set_custom_exec_cb(&anim, _ui_anim_callback_set_image_angle);
        lv_anim_set_values(&anim, 0, 3600);  // 360 degrees * 10
        lv_anim_set_path_cb(&anim, lv_anim_path_linear);
        lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_deleted_cb(&anim, _ui_anim_callback_free_user_data);

        ESP_UTILS_CHECK_FALSE_EXIT(startRecordResource(), "Start record resource failed");
        lv_anim_start(&anim);
        ESP_UTILS_CHECK_FALSE_EXIT(endRecordResource(), "End record resource failed");
    }
}

void ClockApp::updateTime()
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int hour = timeinfo.tm_hour;
    int minute = timeinfo.tm_min;
    int second = timeinfo.tm_sec;

    // Update digital time
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", hour, minute);
    lv_label_set_text(m_label_time, time_str);

    // Update date
    static const char *weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char date_str[32];
    snprintf(date_str, sizeof(date_str), "%s %d %s", 
             weekdays[timeinfo.tm_wday], timeinfo.tm_mday, months[timeinfo.tm_mon]);
    lv_label_set_text(m_label_date, date_str);

    // Calculate angles for hands (in 0.1 degree units, LVGL uses 0.1 degree)
    // Hour: 30 degrees per hour + 0.5 degrees per minute
    int hour_angle = ((hour % 12) * 300 + minute * 5);
    // Minute: 6 degrees per minute + 0.1 degrees per second  
    int min_angle = (minute * 60 + second);
    // Second: 6 degrees per second
    int sec_angle = (second * 60);

    lv_image_set_rotation(m_image_hour, hour_angle);
    lv_image_set_rotation(m_image_min, min_angle);
    lv_image_set_rotation(m_image_sec, sec_angle);
}

void ClockApp::updateTimerCallback(lv_timer_t *timer)
{
    ClockApp *app = (ClockApp *)timer->user_data;
    if (app) {
        app->updateTime();
    }
}

// Register the app with Brookesia system
ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, ClockApp, APP_NAME, []()
{
    return std::shared_ptr<ClockApp>(ClockApp::requestInstance(), [](ClockApp *p) {});
})

} // namespace esp_brookesia::apps

// Force link symbol
extern "C" {
    volatile int esp_brookesia_app_clock_force_link = 0;
}
