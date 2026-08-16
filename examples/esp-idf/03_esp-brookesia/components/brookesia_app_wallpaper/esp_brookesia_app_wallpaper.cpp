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
#define LONG_PRESS_TIME_MS 800     // 长按时间阈值
#define SWIPE_THRESHOLD 50         // 滑动检测阈值

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
    App(APP_NAME, &esp_brookesia_app_icon_launcher_wallpaper_112_112, true, use_status_bar, use_navigation_bar)
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

    // Add touch event handler for gestures and long press
    lv_obj_add_flag(m_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(m_root, touchEventCallback, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(m_root, touchEventCallback, LV_EVENT_PRESSING, this);
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
        
        // 先显示加载提示
        showLoadingHint();
        
        // 延迟加载第一张图，让 UI 先渲染
        lv_timer_create([](lv_timer_t *t) {
            WallpaperApp *app = (WallpaperApp *)t->user_data;
            app->showCurrent();
            lv_timer_delete(t);
        }, 100, this);

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
    
    if (m_long_press_timer) {
        lv_timer_delete(m_long_press_timer);
        m_long_press_timer = nullptr;
    }

    closeSettingsPanel();
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

    // 隐藏加载提示
    hideLoadingHint();

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
    
    // Use LVGL POSIX file path format: "A:" + absolute path
    // The 'A' comes from CONFIG_LV_FS_POSIX_LETTER=65 (ASCII 'A')
    std::string lv_path = "A:" + path;
    ESP_UTILS_LOGI("Loading image: %s", lv_path.c_str());
    
    lv_image_set_src(m_image_obj, lv_path.c_str());
    lv_obj_set_size(m_image_obj, 466, 466);  // Fill screen
    lv_image_set_inner_align(m_image_obj, LV_IMAGE_ALIGN_CENTER);
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
            app->m_touch_time = lv_tick_get();
            app->m_long_press_triggered = false;
            
            // 启动长按检测定时器
            if (app->m_long_press_timer) {
                lv_timer_delete(app->m_long_press_timer);
            }
            app->m_long_press_timer = lv_timer_create(longPressTimerCallback, LONG_PRESS_TIME_MS, app);
            lv_timer_set_repeat_count(app->m_long_press_timer, 1);
        }
    } else if (code == LV_EVENT_PRESSING) {
        // 检查是否移动太多（移动则取消长按）
        if (app->m_touch_active && !app->m_long_press_triggered) {
            lv_indev_t *indev = lv_indev_active();
            if (indev) {
                lv_point_t current;
                lv_indev_get_point(indev, &current);
                int32_t dx = current.x - app->m_touch_start.x;
                int32_t dy = current.y - app->m_touch_start.y;
                
                // 移动超过阈值，取消长按
                if (abs(dx) > 20 || abs(dy) > 20) {
                    if (app->m_long_press_timer) {
                        lv_timer_delete(app->m_long_press_timer);
                        app->m_long_press_timer = nullptr;
                    }
                }
            }
        }
    } else if (code == LV_EVENT_RELEASED && app->m_touch_active) {
        app->m_touch_active = false;
        
        // 删除长按定时器
        if (app->m_long_press_timer) {
            lv_timer_delete(app->m_long_press_timer);
            app->m_long_press_timer = nullptr;
        }
        
        // 如果已经触发了长按，不再处理滑动
        if (app->m_long_press_triggered) {
            return;
        }
        
        lv_indev_t *indev = lv_indev_active();
        if (indev) {
            lv_point_t touch_end;
            lv_indev_get_point(indev, &touch_end);

            int32_t dx = touch_end.x - app->m_touch_start.x;
            int32_t dy = touch_end.y - app->m_touch_start.y;

            // Horizontal swipe detection
            if (abs(dx) > SWIPE_THRESHOLD && abs(dx) > abs(dy)) {
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

void WallpaperApp::longPressTimerCallback(lv_timer_t *timer)
{
    WallpaperApp *app = (WallpaperApp *)timer->user_data;
    if (!app || !app->m_touch_active) return;
    
    app->m_long_press_triggered = true;
    app->m_long_press_timer = nullptr;
    
    ESP_UTILS_LOGI("Long press detected -> open settings");
    app->openSettingsPanel();
}

void WallpaperApp::showLoadingHint()
{
    if (m_loading_label) {
        lv_obj_del(m_loading_label);
    }
    m_loading_label = lv_label_create(m_root);
    lv_label_set_text(m_loading_label, "Loading...");
    lv_obj_set_style_text_color(m_loading_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(m_loading_label, &lv_font_montserrat_20, 0);
    lv_obj_center(m_loading_label);
}

void WallpaperApp::hideLoadingHint()
{
    if (m_loading_label) {
        lv_obj_del(m_loading_label);
        m_loading_label = nullptr;
    }
}

void WallpaperApp::openSettingsPanel()
{
    if (m_settings_panel) return;  // 已经打开
    
    // 暂停自动轮播
    if (m_timer) {
        lv_timer_pause(m_timer);
    }
    
    // 全屏设置面板，参考完整版 wallpaper-app 风格
    m_settings_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(m_settings_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(m_settings_panel, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(m_settings_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_settings_panel, 0, 0);
    lv_obj_set_style_pad_all(m_settings_panel, 0, 0);
    lv_obj_set_style_radius(m_settings_panel, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(m_settings_panel, LV_OBJ_FLAG_SCROLLABLE);
    
    // 标题
    lv_obj_t *title = lv_label_create(m_settings_panel);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);
    
    // 可滚动内容区域
    lv_obj_t *content = lv_obj_create(m_settings_panel);
    lv_obj_set_size(content, 280, 280);
    lv_obj_align(content, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 5, 0);
    lv_obj_set_style_pad_row(content, 10, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
    
    // 创建设置行的辅助 lambda
    auto create_row = [&](const char *label_text, const char *value_text) -> lv_obj_t* {
        lv_obj_t *row = lv_obj_create(content);
        lv_obj_set_size(row, 260, 50);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x16213e), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 12, 0);
        lv_obj_set_style_pad_hor(row, 12, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        
        lv_obj_t *label = lv_label_create(row);
        lv_label_set_text(label, label_text);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
        
        lv_obj_t *value = lv_label_create(row);
        lv_label_set_text(value, value_text);
        lv_obj_set_style_text_color(value, lv_color_hex(0x00d4ff), 0);
        lv_obj_set_style_text_font(value, &lv_font_montserrat_16, 0);
        lv_obj_align(value, LV_ALIGN_RIGHT_MID, 0, 0);
        
        return row;
    };
    
    // Interval 行
    char interval_buf[16];
    snprintf(interval_buf, sizeof(interval_buf), "%lu sec", (unsigned long)(m_interval_ms / 1000));
    create_row("Interval", interval_buf);
    
    // Wallpapers 行
    char count_buf[16];
    snprintf(count_buf, sizeof(count_buf), "%d", (int)m_wallpapers.size());
    create_row("Wallpapers", count_buf);
    
    // Current 行
    char current_buf[16];
    snprintf(current_buf, sizeof(current_buf), "%d / %d", m_current_index + 1, (int)m_wallpapers.size());
    create_row("Current", current_buf);
    
    // 提示行
    lv_obj_t *hint_row = lv_obj_create(content);
    lv_obj_set_size(hint_row, 260, 50);
    lv_obj_set_style_bg_color(hint_row, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_bg_opa(hint_row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hint_row, 0, 0);
    lv_obj_set_style_radius(hint_row, 12, 0);
    lv_obj_clear_flag(hint_row, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *hint = lv_label_create(hint_row);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Swipe " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_center(hint);
    
    // 关闭按钮
    lv_obj_t *close_btn = lv_btn_create(content);
    lv_obj_set_size(close_btn, 260, 50);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x444444), 0);
    lv_obj_set_style_radius(close_btn, 12, 0);
    lv_obj_add_event_cb(close_btn, [](lv_event_t *e) {
        WallpaperApp *app = (WallpaperApp *)lv_event_get_user_data(e);
        app->closeSettingsPanel();
    }, LV_EVENT_CLICKED, this);
    
    lv_obj_t *btn_label = lv_label_create(close_btn);
    lv_label_set_text(btn_label, "Close");
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_16, 0);
    lv_obj_center(btn_label);
}

void WallpaperApp::closeSettingsPanel()
{
    if (m_settings_panel) {
        lv_obj_del(m_settings_panel);
        m_settings_panel = nullptr;
        
        // 恢复自动轮播
        if (m_timer) {
            lv_timer_resume(m_timer);
            lv_timer_reset(m_timer);
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
