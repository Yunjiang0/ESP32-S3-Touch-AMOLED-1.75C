/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "bsp/esp-bsp.h"
#include "esp_brookesia.hpp"
#include "boost/thread.hpp"
#include "esp_littlefs.h"
#include <sys/stat.h>
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "Main"
#include "esp_lib_utils.h"

// Force link wallpaper app component
#include "esp_brookesia_app_wallpaper.hpp"
extern "C" { extern volatile int esp_brookesia_app_wallpaper_force_link; }

// Force link clock app component
#include "esp_brookesia_app_clock.hpp"
extern "C" { extern volatile int esp_brookesia_app_clock_force_link; }

// 466x466 round display stylesheet
#include "stylesheets/466_466/dark/stylesheet.hpp"

using namespace esp_brookesia;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems::phone;

constexpr bool EXAMPLE_SHOW_MEM_INFO = false;
constexpr uint32_t LVGL_TASK_STACK_SIZE = 40 * 1024;

static void mount_littlefs(void)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/storage",
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret == ESP_OK) {
        ESP_UTILS_LOGI("LittleFS mounted successfully");
        
        // Create wallpapers directory if it doesn't exist
        struct stat st;
        if (stat("/storage/wallpapers", &st) != 0) {
            mkdir("/storage/wallpapers", 0755);
            ESP_UTILS_LOGI("Created /storage/wallpapers directory");
        }
    } else if (ret == ESP_ERR_INVALID_STATE) {
        ESP_UTILS_LOGI("LittleFS already mounted");
    } else {
        ESP_UTILS_LOGE("Failed to mount LittleFS: %s", esp_err_to_name(ret));
    }
}

extern "C" void app_main(void)
{
    // Force link the wallpaper app
    (void)esp_brookesia_app_wallpaper_force_link;
    // Force link the clock app
    (void)esp_brookesia_app_clock_force_link;

    ESP_UTILS_LOGI("Display ESP-Brookesia phone demo");

    // Mount LittleFS before starting LVGL (must be done in main task, not LVGL task)
    mount_littlefs();

    /* Brookesia screen creation exceeds the adapter's 8 KB default on this board. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_lv_adapter_config_t lv_adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
#pragma GCC diagnostic pop
    lv_adapter_config.task_stack_size = LVGL_TASK_STACK_SIZE;
    lv_adapter_config.stack_in_psram = true;

    bsp_display_cfg_t display_config = {};
    display_config.lv_adapter_cfg = lv_adapter_config;
    display_config.rotation = ESP_LV_ADAPTER_ROTATE_0;
    display_config.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE;
    display_config.touch_flags.mirror_x = 1;
    display_config.touch_flags.mirror_y = 1;

    ESP_UTILS_CHECK_NULL_EXIT(
        bsp_display_start_with_config(&display_config), "Start display failed"
    );
    ESP_UTILS_CHECK_ERROR_EXIT(bsp_display_backlight_on(), "Turn on display backlight failed");

    /* Configure GUI lock */
    LvLock::registerCallbacks([](int timeout_ms) {
        esp_err_t ret = bsp_display_lock(timeout_ms);
        ESP_UTILS_CHECK_FALSE_RETURN(ret == ESP_OK, false, "Lock failed (timeout_ms: %d)", timeout_ms);

        return true;
    }, []() {
        bsp_display_unlock();
        return true;
    });

    /* Create a phone object */
    Phone *phone = new (std::nothrow) Phone();
    ESP_UTILS_CHECK_NULL_EXIT(phone, "Create phone failed");

    {
        // When operating on non-GUI tasks, should acquire a lock before operating on LVGL
        LvLockGuard gui_guard;

        /* Use 466x466 dark stylesheet for round display */
        ESP_UTILS_CHECK_FALSE_EXIT(phone->addStylesheet(ESP_BROOKESIA_PHONE_466_466_DARK_STYLESHEET()), "Add stylesheet failed");
        ESP_UTILS_CHECK_FALSE_EXIT(phone->activateStylesheet(ESP_BROOKESIA_PHONE_466_466_DARK_STYLESHEET()), "Activate stylesheet failed");

        /* Begin the phone */
        ESP_UTILS_CHECK_FALSE_EXIT(phone->begin(), "Begin failed");
        // assert(phone->getDisplay().showContainerBorder() && "Show container border failed");

        /* Init and install apps from registry */
        std::vector<systems::base::Manager::RegistryAppInfo> inited_apps;
        ESP_UTILS_CHECK_FALSE_EXIT(phone->initAppFromRegistry(inited_apps), "Init app registry failed");
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installAppFromRegistry(inited_apps), "Install app registry failed");

        /* Create a timer to update the clock */
        lv_timer_create([](lv_timer_t *t) {
            time_t now;
            struct tm timeinfo;
            Phone *phone = (Phone *)t->user_data;

            ESP_UTILS_CHECK_NULL_EXIT(phone, "Invalid phone");

            time(&now);
            localtime_r(&now, &timeinfo);

            ESP_UTILS_CHECK_FALSE_EXIT(
                phone->getDisplay().getStatusBar()->setClock(timeinfo.tm_hour, timeinfo.tm_min),
                "Refresh status bar failed"
            );
        }, 1000, phone);

        /* Initialize status bar icons */
        // Set battery to 100% (no real battery on this board, just for display)
        phone->getDisplay().getStatusBar()->setBatteryPercent(false, 100);
        // Set WiFi icon to closed (will be updated when WiFi is connected)
        phone->getDisplay().getStatusBar()->setWifiIconState(StatusBar::WifiState::CLOSED);
    }

    if constexpr (EXAMPLE_SHOW_MEM_INFO) {
        esp_utils::thread_config_guard thread_config({
            .name = "mem_info",
            .stack_size = 4096,
        });
        boost::thread([ = ]() {
            char buffer[128];    /* Make sure buffer is enough for `sprintf` */
            size_t internal_free = 0;
            size_t internal_total = 0;
            size_t external_free = 0;
            size_t external_total = 0;

            while (1) {
                internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
                internal_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
                external_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
                external_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
                sprintf(buffer,
                        "\t           Biggest /     Free /    Total\n"
                        "\t  SRAM : [%8d / %8d / %8d]\n"
                        "\t PSRAM : [%8d / %8d / %8d]",
                        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), internal_free, internal_total,
                        heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM), external_free, external_total);
                ESP_UTILS_LOGI("\n%s", buffer);

                {
                    LvLockGuard gui_guard;
                    ESP_UTILS_CHECK_FALSE_EXIT(
                        phone->getDisplay().getRecentsScreen()->setMemoryLabel(
                            internal_free / 1024, internal_total / 1024, external_free / 1024, external_total / 1024
                        ), "Set memory label failed"
                    );
                }

                boost::this_thread::sleep_for(boost::chrono::seconds(5));
            }
        }).detach();
    }
}