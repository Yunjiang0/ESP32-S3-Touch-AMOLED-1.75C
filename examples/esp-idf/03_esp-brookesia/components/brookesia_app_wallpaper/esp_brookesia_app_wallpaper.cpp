/*
 * SPDX-FileCopyrightText: 2024 Custom
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstring>
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "esp_littlefs.h"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Wallpaper"
#include "esp_lib_utils.h"
#include "esp_brookesia_app_wallpaper.hpp"

#define APP_NAME "Wallpaper"
#define WALLPAPER_DIR "/storage/wallpapers"
#define MAX_GIF_SIZE (500 * 1024)  // 500KB limit for GIF

using namespace std;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems;

// Declare the app icon (112x112 ARGB8888)
LV_IMG_DECLARE(esp_brookesia_app_icon_launcher_wallpaper_112_112);

namespace esp_brookesia::apps {

WallpaperApp *WallpaperApp::_instance = nullptr;

WallpaperApp *WallpaperApp::requestInstance(bool use_status_bar, bool use_navigation_bar)
{
    if (_instance == nullptr) {
        _instance = new WallpaperApp(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

WallpaperApp::WallpaperApp(bool use_status_bar, bool use_navigation_bar):
    App(APP_NAME, &esp_brookesia_app_icon_launcher_wallpaper_112_112, false, use_status_bar, use_navigation_bar)
{
}

WallpaperApp::~WallpaperApp()
{
    clearPlayer();
}

bool WallpaperApp::run(void)
{
    ESP_UTILS_LOGD("Run");

    // NOTE: LittleFS must be mounted before this app runs (in main.cpp)
    // Mounting here would crash because we're in the LVGL task context

    // Create root container
    m_root = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(m_root);
    lv_obj_set_size(m_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(m_root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(m_root, LV_OPA_COVER, 0);
    lv_obj_center(m_root);

    // Add touch event handler
    lv_obj_add_flag(m_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(m_root, touchEventCallback, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(m_root, touchEventCallback, LV_EVENT_RELEASED, this);

    // Scan wallpapers
    scanWallpapers();

    if (m_wallpapers.empty()) {
        // No wallpapers found, show message
        lv_obj_t *label = lv_label_create(m_root);
        lv_label_set_text(label, "No wallpapers found\n\nUpload images to:\n/storage/wallpapers/");
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);
    } else {
        ESP_UTILS_LOGI("Found %d wallpapers", (int)m_wallpapers.size());
        showCurrent();

        // Create auto-rotation timer
        m_timer = lv_timer_create(timerCallback, m_interval_ms, this);
    }

    return true;
}

bool WallpaperApp::back(void)
{
    ESP_UTILS_LOGD("Back");
    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
    return true;
}

bool WallpaperApp::close(void)
{
    ESP_UTILS_LOGD("Close");

    if (m_timer) {
        lv_timer_delete(m_timer);
        m_timer = nullptr;
    }

    clearPlayer();

    return true;
}

bool WallpaperApp::pause(void)
{
    ESP_UTILS_LOGD("Pause");
    if (m_timer) {
        lv_timer_pause(m_timer);
    }
    return true;
}

bool WallpaperApp::resume(void)
{
    ESP_UTILS_LOGD("Resume");
    if (m_timer) {
        lv_timer_resume(m_timer);
        lv_timer_reset(m_timer);
    }
    return true;
}

void WallpaperApp::scanWallpapers()
{
    m_wallpapers.clear();

    // Ensure directory exists
    struct stat st;
    if (stat(WALLPAPER_DIR, &st) != 0) {
        mkdir(WALLPAPER_DIR, 0755);
        return;
    }

    DIR *dir = opendir(WALLPAPER_DIR);
    if (!dir) {
        ESP_UTILS_LOGE("Failed to open wallpaper directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_REG) continue;

        std::string name = entry->d_name;
        // Check file extension
        size_t dot = name.rfind('.');
        if (dot == std::string::npos) continue;

        std::string ext = name.substr(dot);
        // Convert to lowercase
        for (auto &c : ext) c = tolower(c);

        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || 
            ext == ".bmp" || ext == ".gif") {
            m_wallpapers.push_back(std::string(WALLPAPER_DIR) + "/" + name);
        }
    }
    closedir(dir);

    // Sort alphabetically
    std::sort(m_wallpapers.begin(), m_wallpapers.end());
}

bool WallpaperApp::isGif(const std::string &path)
{
    size_t dot = path.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = path.substr(dot);
    for (auto &c : ext) c = tolower(c);
    return ext == ".gif";
}

void WallpaperApp::clearPlayer()
{
    if (m_image_obj) {
        lv_obj_del(m_image_obj);
        m_image_obj = nullptr;
    }
    if (m_player_obj) {
        lv_obj_del(m_player_obj);
        m_player_obj = nullptr;
    }
    if (m_player_buf) {
        heap_caps_free(m_player_buf);
        m_player_buf = nullptr;
    }
}

void WallpaperApp::showCurrent()
{
    if (m_wallpapers.empty()) return;

    const std::string &path = m_wallpapers[m_current_index];
    ESP_UTILS_LOGI("Showing: %s (%d/%d)", path.c_str(), m_current_index + 1, (int)m_wallpapers.size());

    if (isGif(path)) {
        showGif(path);
    } else {
        showImage(path);
    }
}

void WallpaperApp::showImage(const std::string &path)
{
    clearPlayer();

    m_image_obj = lv_image_create(m_root);
    
    // Use LVGL file path format
    std::string lv_path = "A:" + path;
    lv_image_set_src(m_image_obj, lv_path.c_str());
    lv_obj_center(m_image_obj);

    // Allow gestures to pass through
    lv_obj_add_flag(m_image_obj, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_clear_flag(m_image_obj, LV_OBJ_FLAG_CLICKABLE);
}

void WallpaperApp::showGif(const std::string &path)
{
    clearPlayer();

    // Check file size
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        ESP_UTILS_LOGE("GIF file not found: %s", path.c_str());
        return;
    }

    if ((size_t)st.st_size > MAX_GIF_SIZE) {
        ESP_UTILS_LOGW("GIF too large (%ld bytes), showing placeholder", (long)st.st_size);
        m_player_obj = lv_label_create(m_root);
        lv_label_set_text(m_player_obj, "GIF too large\n(max 500KB)");
        lv_obj_set_style_text_color(m_player_obj, lv_color_white(), 0);
        lv_obj_set_style_text_align(m_player_obj, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(m_player_obj);
        return;
    }

    // Read GIF into memory
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) {
        ESP_UTILS_LOGE("Failed to open GIF: %s", path.c_str());
        return;
    }

    size_t file_size = st.st_size;
    size_t total_size = sizeof(lv_image_dsc_t) + file_size;
    
    uint8_t *buf = (uint8_t *)heap_caps_malloc(total_size, MALLOC_CAP_SPIRAM);
    if (!buf) {
        ESP_UTILS_LOGE("Failed to allocate memory for GIF");
        fclose(f);
        return;
    }

    lv_image_dsc_t *img_dsc = (lv_image_dsc_t *)buf;
    uint8_t *gif_data = buf + sizeof(lv_image_dsc_t);

    if (fread(gif_data, 1, file_size, f) != file_size) {
        ESP_UTILS_LOGE("Failed to read GIF file");
        fclose(f);
        heap_caps_free(buf);
        return;
    }
    fclose(f);

    // Verify GIF header
    if (memcmp(gif_data, "GIF87a", 6) != 0 && memcmp(gif_data, "GIF89a", 6) != 0) {
        ESP_UTILS_LOGE("Invalid GIF header");
        heap_caps_free(buf);
        return;
    }

    m_player_buf = buf;

    // Setup image descriptor
    memset(img_dsc, 0, sizeof(lv_image_dsc_t));
    img_dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    img_dsc->header.cf = LV_COLOR_FORMAT_RAW;
    img_dsc->data_size = file_size;
    img_dsc->data = gif_data;

    // Create GIF widget
    m_player_obj = lv_gif_create(m_root);
    lv_gif_set_color_format(m_player_obj, LV_COLOR_FORMAT_RGB565);
    lv_obj_add_flag(m_player_obj, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_clear_flag(m_player_obj, LV_OBJ_FLAG_CLICKABLE);

    lv_gif_set_src(m_player_obj, img_dsc);

    if (!lv_gif_is_loaded(m_player_obj)) {
        ESP_UTILS_LOGW("GIF decode failed");
        lv_obj_del(m_player_obj);
        m_player_obj = lv_label_create(m_root);
        lv_label_set_text(m_player_obj, "GIF decode failed");
        lv_obj_set_style_text_color(m_player_obj, lv_color_white(), 0);
        lv_obj_center(m_player_obj);
        heap_caps_free(m_player_buf);
        m_player_buf = nullptr;
        return;
    }

    lv_obj_center(m_player_obj);
}

void WallpaperApp::showNext()
{
    if (m_wallpapers.empty()) return;
    m_current_index = (m_current_index + 1) % m_wallpapers.size();
    showCurrent();
    if (m_timer) lv_timer_reset(m_timer);
}

void WallpaperApp::showPrev()
{
    if (m_wallpapers.empty()) return;
    m_current_index = (m_current_index - 1 + m_wallpapers.size()) % m_wallpapers.size();
    showCurrent();
    if (m_timer) lv_timer_reset(m_timer);
}

void WallpaperApp::timerCallback(lv_timer_t *timer)
{
    WallpaperApp *app = (WallpaperApp *)timer->user_data;
    if (app) {
        app->showNext();
    }
}

void WallpaperApp::touchEventCallback(lv_event_t *e)
{
    WallpaperApp *app = (WallpaperApp *)lv_event_get_user_data(e);
    if (!app) return;

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        lv_indev_t *indev = lv_indev_active();
        if (indev) {
            lv_indev_get_point(indev, &app->m_touch_start);
            app->m_touch_active = true;
        }
    } else if (code == LV_EVENT_RELEASED && app->m_touch_active) {
        app->m_touch_active = false;
        
        lv_indev_t *indev = lv_indev_active();
        if (indev) {
            lv_point_t touch_end;
            lv_indev_get_point(indev, &touch_end);

            int32_t dx = touch_end.x - app->m_touch_start.x;
            int32_t dy = touch_end.y - app->m_touch_start.y;

            // Horizontal swipe detection (threshold 50px)
            if (abs(dx) > 50 && abs(dx) > abs(dy)) {
                if (dx < 0) {
                    ESP_UTILS_LOGI("Swipe LEFT -> next");
                    app->showNext();
                } else {
                    ESP_UTILS_LOGI("Swipe RIGHT -> prev");
                    app->showPrev();
                }
            }
        }
    }
}

// Register the app with Brookesia system
ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, WallpaperApp, APP_NAME, []()
{
    return std::shared_ptr<WallpaperApp>(WallpaperApp::requestInstance(), [](WallpaperApp *p) {});
})

} // namespace esp_brookesia::apps

// Force link symbol - must be referenced from main to prevent linker from discarding
extern "C" {
    volatile int esp_brookesia_app_wallpaper_force_link = 0;
}
