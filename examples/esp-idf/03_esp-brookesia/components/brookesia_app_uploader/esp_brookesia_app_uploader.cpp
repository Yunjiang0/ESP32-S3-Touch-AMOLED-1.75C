/*
 * SPDX-FileCopyrightText: 2024 Custom
 * SPDX-License-Identifier: Apache-2.0
 */
#include <dirent.h>
#include <sys/stat.h>
#include "lvgl.h"
#include "esp_brookesia.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Uploader"
#include "esp_lib_utils.h"
#include "esp_brookesia_app_uploader.hpp"
#include "wallpaper_uploader.h"

#define APP_NAME "Uploader"
#define WALLPAPER_DIR "/storage/wallpapers"

using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems;

LV_IMG_DECLARE(esp_brookesia_app_icon_launcher_uploader_112_112);

namespace esp_brookesia::apps {

UploaderApp *UploaderApp::_instance = nullptr;

UploaderApp *UploaderApp::requestInstance(bool use_status_bar, bool use_navigation_bar)
{
    if (_instance == nullptr) {
        _instance = new UploaderApp(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

UploaderApp::UploaderApp(bool use_status_bar, bool use_navigation_bar):
    App(APP_NAME, &esp_brookesia_app_icon_launcher_uploader_112_112, true, use_status_bar, use_navigation_bar)
{
}

UploaderApp::~UploaderApp()
{
    stopWiFi();
}

bool UploaderApp::run(void)
{
    ESP_UTILS_LOGD("Run");

    // 创建根容器
    m_root = lv_obj_create(lv_scr_act());
    lv_obj_set_size(m_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(m_root, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(m_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_root, 0, 0);
    lv_obj_set_style_pad_all(m_root, 0, 0);
    lv_obj_set_style_radius(m_root, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(m_root, LV_OBJ_FLAG_SCROLLABLE);

    // 标题
    lv_obj_t *title = lv_label_create(m_root);
    lv_label_set_text(title, LV_SYMBOL_WIFI " WiFi Upload");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    // 状态标签
    m_status_label = lv_label_create(m_root);
    lv_label_set_text(m_status_label, "WiFi: OFF");
    lv_obj_set_style_text_color(m_status_label, lv_color_hex(0xff6b6b), 0);
    lv_obj_set_style_text_font(m_status_label, &lv_font_montserrat_18, 0);
    lv_obj_align(m_status_label, LV_ALIGN_CENTER, 0, -60);

    // 连接信息
    m_info_label = lv_label_create(m_root);
    lv_label_set_text(m_info_label, "");
    lv_obj_set_style_text_color(m_info_label, lv_color_hex(0x00d4ff), 0);
    lv_obj_set_style_text_font(m_info_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(m_info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(m_info_label, LV_ALIGN_CENTER, 0, -20);

    // 文件数量
    m_count_label = lv_label_create(m_root);
    lv_obj_set_style_text_color(m_count_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(m_count_label, &lv_font_montserrat_14, 0);
    lv_obj_align(m_count_label, LV_ALIGN_CENTER, 0, 20);

    // 启动按钮
    m_start_btn = lv_btn_create(m_root);
    lv_obj_set_size(m_start_btn, 200, 60);
    lv_obj_align(m_start_btn, LV_ALIGN_CENTER, 0, 80);
    lv_obj_set_style_bg_color(m_start_btn, lv_color_hex(0x00d4ff), 0);
    lv_obj_set_style_radius(m_start_btn, 15, 0);
    lv_obj_add_event_cb(m_start_btn, [](lv_event_t *e) {
        UploaderApp *app = (UploaderApp *)lv_event_get_user_data(e);
        app->startWiFi();
    }, LV_EVENT_CLICKED, this);

    lv_obj_t *start_lbl = lv_label_create(m_start_btn);
    lv_label_set_text(start_lbl, "Start WiFi");
    lv_obj_set_style_text_color(start_lbl, lv_color_black(), 0);
    lv_obj_set_style_text_font(start_lbl, &lv_font_montserrat_18, 0);
    lv_obj_center(start_lbl);

    // 停止按钮 (初始隐藏)
    m_stop_btn = lv_btn_create(m_root);
    lv_obj_set_size(m_stop_btn, 200, 60);
    lv_obj_align(m_stop_btn, LV_ALIGN_CENTER, 0, 80);
    lv_obj_set_style_bg_color(m_stop_btn, lv_color_hex(0xe94560), 0);
    lv_obj_set_style_radius(m_stop_btn, 15, 0);
    lv_obj_add_flag(m_stop_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(m_stop_btn, [](lv_event_t *e) {
        UploaderApp *app = (UploaderApp *)lv_event_get_user_data(e);
        app->stopWiFi();
    }, LV_EVENT_CLICKED, this);

    lv_obj_t *stop_lbl = lv_label_create(m_stop_btn);
    lv_label_set_text(stop_lbl, "Stop WiFi");
    lv_obj_set_style_text_color(stop_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(stop_lbl, &lv_font_montserrat_18, 0);
    lv_obj_center(stop_lbl);

    // 定时刷新文件数量
    m_refresh_timer = lv_timer_create(refreshTimerCallback, 2000, this);

    updateUI();
    return true;
}

bool UploaderApp::back(void)
{
    ESP_UTILS_LOGD("Back");
    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
    return true;
}

bool UploaderApp::close(void)
{
    ESP_UTILS_LOGD("Close");

    if (m_refresh_timer) {
        lv_timer_delete(m_refresh_timer);
        m_refresh_timer = nullptr;
    }

    // 注意：不自动停止 WiFi，让用户手动停止
    return true;
}

bool UploaderApp::pause(void)
{
    ESP_UTILS_LOGD("Pause");
    if (m_refresh_timer) {
        lv_timer_pause(m_refresh_timer);
    }
    return true;
}

bool UploaderApp::resume(void)
{
    ESP_UTILS_LOGD("Resume");
    if (m_refresh_timer) {
        lv_timer_resume(m_refresh_timer);
    }
    updateUI();
    return true;
}

void UploaderApp::startWiFi()
{
    if (m_wifi_started) return;

    wallpaper_uploader_config_t cfg = WALLPAPER_UPLOADER_DEFAULT_CONFIG();
    if (wallpaper_uploader_start(&cfg)) {
        m_wifi_started = true;
        ESP_UTILS_LOGI("WiFi AP started");
    } else {
        ESP_UTILS_LOGE("WiFi AP start failed");
    }
    updateUI();
}

void UploaderApp::stopWiFi()
{
    if (!m_wifi_started) return;

    wallpaper_uploader_stop();
    m_wifi_started = false;
    ESP_UTILS_LOGI("WiFi AP stopped");
    updateUI();
}

void UploaderApp::updateUI()
{
    if (m_wifi_started) {
        lv_label_set_text(m_status_label, "WiFi: ON");
        lv_obj_set_style_text_color(m_status_label, lv_color_hex(0x00cc00), 0);
        lv_label_set_text(m_info_label, "SSID: Wallpaper-AP\nPass: 12345678\nOpen: 192.168.4.1");
        lv_obj_add_flag(m_start_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(m_stop_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(m_status_label, "WiFi: OFF");
        lv_obj_set_style_text_color(m_status_label, lv_color_hex(0xff6b6b), 0);
        lv_label_set_text(m_info_label, "");
        lv_obj_clear_flag(m_start_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(m_stop_btn, LV_OBJ_FLAG_HIDDEN);
    }

    // 更新文件数量
    size_t count = wallpaper_uploader_get_file_count();
    char buf[32];
    snprintf(buf, sizeof(buf), "Wallpapers: %zu", count);
    lv_label_set_text(m_count_label, buf);
}

void UploaderApp::refreshTimerCallback(lv_timer_t *timer)
{
    UploaderApp *app = (UploaderApp *)timer->user_data;
    if (app) {
        app->updateUI();
    }
}

// 注册应用
ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, UploaderApp, APP_NAME, []()
{
    return std::shared_ptr<UploaderApp>(UploaderApp::requestInstance(), [](UploaderApp *p) {});
})

} // namespace esp_brookesia::apps

// 强制链接符号
extern "C" {
    volatile int esp_brookesia_app_uploader_force_link = 0;
}
