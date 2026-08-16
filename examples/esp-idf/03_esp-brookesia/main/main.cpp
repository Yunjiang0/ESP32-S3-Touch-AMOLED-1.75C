/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "bsp/esp-bsp.h"
#include "esp_brookesia.hpp"
#include "boost/thread.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "Main"
#include "esp_lib_utils.h"

// Force link wallpaper app component
#include "esp_brookesia_app_wallpaper.hpp"

using namespace esp_brookesia;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems::phone;

constexpr bool EXAMPLE_SHOW_MEM_INFO = false;
constexpr uint32_t LVGL_TASK_STACK_SIZE = 40 * 1024;

// 466x466 圆形屏幕样式表 (基于 480x480 修改)
namespace {

constexpr StatusBar::AreaData STATUS_BAR_AREA_DATA(int w_percent, StatusBar::AreaAlign align)
{
    return {
        .size = gui::StyleSize::RECT_PERCENT(w_percent, 100),
        .layout_column_align = align,
        .layout_column_start_offset = 30,  // 增加边距，避免圆角裁剪
        .layout_column_pad = 4,
    };
}

constexpr StatusBar::Data STATUS_BAR_DATA = {
    .main = {
        .size = gui::StyleSize::RECT_W_PERCENT(100, 36),
        .background_color = gui::StyleColor::COLOR_WITH_OPACITY(0x000000, 0),  // 透明背景
        .text_font = gui::StyleFont::SIZE(16),
        .text_color = gui::StyleColor::COLOR(0xFFFFFF),
    },
    .area = {
        .num = 2,
        .data = {
            STATUS_BAR_AREA_DATA(50, StatusBar::AreaAlign::START),
            STATUS_BAR_AREA_DATA(50, StatusBar::AreaAlign::END),
        },
    },
    .icon_common_size = gui::StyleSize::SQUARE(20),
    .battery = {
        .area_index = 1,
        .icon_data = {
            .icon = {
                .image_num = 5,
                .images = {
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_middle_status_bar_battery_level1_24_24),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_middle_status_bar_battery_level2_24_24),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_middle_status_bar_battery_level3_24_24),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_middle_status_bar_battery_level4_24_24),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_middle_status_bar_battery_charge_24_24),
                },
            },
        },
    },
    .wifi = {
        .area_index = 1,
        .icon_data = {
            .icon = {
                .image_num = 4,
                .images = {
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_middle_status_bar_wifi_close_24_24),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_middle_status_bar_wifi_level1_24_24),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_middle_status_bar_wifi_level2_24_24),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_middle_status_bar_wifi_level3_24_24),
                },
            },
        },
    },
    .clock = {
        .area_index = 0,
    },
    .flags = {
        .enable_battery_icon = 1,
        .enable_battery_icon_common_size = 1,
        .enable_battery_label = 1,
        .enable_wifi_icon = 1,
        .enable_wifi_icon_common_size = 1,
        .enable_clock = 1,
    },
};

constexpr NavigationBar::Data NAVIGATION_BAR_DATA = {
    .main = {
        .size = gui::StyleSize::RECT_W_PERCENT(100, 36),
        .background_color = gui::StyleColor::COLOR_WITH_OPACITY(0x000000, 0),  // 透明
    },
    .flags = {
        .enable_main_background_color = 0,
    },
};

constexpr AppLauncher::Data APP_LAUNCHER_DATA = {
    .main = {
        .y_start = 50,  // 从顶部偏移，避免圆角
        .size = gui::StyleSize::RECT_PERCENT(100, 100),
        .background_color = gui::StyleColor::COLOR(0x1A1A1A),
        .radius = 233,  // 圆形屏幕半径
        .pad_inner = 30,  // 内边距
    },
    .table = {
        .default_num = 0,
        .size = gui::StyleSize::RECT_PERCENT(85, 70),  // 缩小显示区域
        .layout_row_pad = 15,
        .layout_column_pad = 10,
        .y_offset = 10,
    },
    .spot = {
        .main = {
            .size = gui::StyleSize::RECT(40, 16),
            .pad_bottom = 40,  // 底部边距增大
        },
        .indicator = {
            .main = {
                .diameter = 6,
                .inactive_background_color = gui::StyleColor::COLOR(0x808080),
                .active_background_color = gui::StyleColor::COLOR(0xFFFFFF),
            },
            .main_layout_pad_column = 8,
        },
    },
    .app_icon = {
        .main = {
            .size = gui::StyleSize::RECT(90, 110),
        },
        .image = {
            .size = gui::StyleSize::SQUARE_PERCENT(80),
            .radius = 20,
        },
        .label = {
            .text_font = gui::StyleFont::SIZE(14),
            .text_color = gui::StyleColor::COLOR(0xFFFFFF),
        },
    },
    .flags = {
        .enable_table_scroll_anim = 1,
        .enable_app_icon_image_background_color = 0,
    },
};

constexpr RecentsScreen::Data RECENTS_SCREEN_DATA = {
    .main = {
        .y_start = 50,
        .size = gui::StyleSize::RECT_PERCENT(100, 100),
        .background_color = gui::StyleColor::COLOR(0x1A1A1A),
        .radius = 233,
    },
    .memory_label = {
        .main = {
            .text_font = gui::StyleFont::SIZE(14),
            .text_color = gui::StyleColor::COLOR(0xCCCCCC),
        },
        .format = {
            .type = RecentsScreen::MemoryLabelFormatType::USAGE_PERCENT,
        },
    },
    .snapshot_table = {
        .main = {
            .y_offset = 30,
            .size = gui::StyleSize::RECT_PERCENT(85, 70),
            .layout_pad_column = 10,
        },
    },
    .snapshot = {
        .main = {
            .size = gui::StyleSize::RECT(280, 280),
            .background_color = gui::StyleColor::COLOR(0x38393A),
            .radius = 20,
        },
        .title_height = 32,
        .title_icon_size = gui::StyleSize::SQUARE(24),
        .title_text_font = gui::StyleFont::SIZE(14),
        .title_text_color = gui::StyleColor::COLOR(0xFFFFFF),
        .image_default_resource = gui::StyleImage::SYMBOL(LV_SYMBOL_IMAGE),
    },
    .trash_icon = {
        .main = {
            .y_offset = 10,
            .size = gui::StyleSize::SQUARE(40),
            .image_recolor = gui::StyleColor::COLOR(0xFFFFFF),
            .inactive_image = gui::StyleImage::SYMBOL(LV_SYMBOL_CLOSE),
            .active_image = gui::StyleImage::SYMBOL(LV_SYMBOL_TRASH),
        },
    },
    .flags = {
        .enable_memory_label = 1,
        .enable_table_scroll_anim = 1,
        .enable_snapshot_image_background_color = 0,
        .enable_trash_icon = 1,
    },
};

constexpr Manager::GestureData GESTURE_DATA = {
    .indicator = {
        .size = gui::StyleSize::RECT(40, 4),
        .radius = 2,
        .y_offset = 8,
        .background_color = gui::StyleColor::COLOR(0xFFFFFF),
    },
    .mask = {
        .hor_edge_size = gui::StyleSize::RECT(20, 466),
        .navigation_back_color = gui::StyleColor::COLOR(0x303030),
        .threshold = {
            .direction_vertical = 20,
            .direction_horizontal = 30,
            .top_edge_trigger_navigation_back = 50,
            .bottom_edge_trigger_navigation_back = 50,
        },
    },
    .navigate_color = {
        .bar = gui::StyleColor::COLOR(0xFFFFFF),
        .circle = gui::StyleColor::COLOR(0x303030),
    },
};

constexpr Display::Data DISPLAY_DATA = {
    .status_bar = {
        .data = STATUS_BAR_DATA,
        .visual_mode = StatusBar::VisualMode::SHOW_FIXED,
    },
    .navigation_bar = {
        .data = NAVIGATION_BAR_DATA,
        .visual_mode = NavigationBar::VisualMode::HIDE,
    },
    .app_launcher = {
        .data = APP_LAUNCHER_DATA,
        .default_image = gui::StyleImage::IMAGE(&esp_brookesia_image_middle_app_launcher_default_112_112),
    },
    .recents_screen = {
        .data = RECENTS_SCREEN_DATA,
        .status_bar_visual_mode = StatusBar::VisualMode::HIDE,
        .navigation_bar_visual_mode = NavigationBar::VisualMode::HIDE,
    },
    .flags = {
        .enable_status_bar = 1,
        .enable_navigation_bar = 0,
        .enable_app_launcher_flex_size = 1,
        .enable_recents_screen = 1,
        .enable_recents_screen_flex_size = 1,
    },
};

constexpr Manager::Data MANAGER_DATA = {
    .gesture = GESTURE_DATA,
    .gesture_mask_indicator_trigger_time_ms = 0,
    .recents_screen = {
        .drag_snapshot_y_step = 10,
        .drag_snapshot_y_threshold = 50,
        .drag_snapshot_angle_threshold = 60,
        .delete_snapshot_y_threshold = 50,
    },
    .flags = {
        .enable_gesture = 1,
        .enable_gesture_navigation_back = 0,
        .enable_recents_screen_snapshot_drag = 1,
        .enable_recents_screen_hide_when_no_snapshot = 1,
    },
};

constexpr base::Display::Data CORE_DISPLAY_DATA = {
    .background = {
        .color = gui::StyleColor::COLOR(0x000000),
    },
};

constexpr base::Core::Data CORE_DATA = {
    .name = "466x466 Round Dark",
    .screen_size = gui::StyleSize::SQUARE(466),
    .display = CORE_DISPLAY_DATA,
};

constexpr Stylesheet STYLESHEET_466_466_ROUND_DARK = {
    .core = CORE_DATA,
    .display = DISPLAY_DATA,
    .manager = MANAGER_DATA,
};

} // anonymous namespace

extern "C" void app_main(void)
{
    ESP_UTILS_LOGI("Display ESP-Brookesia phone demo");

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

        /* Add and activate custom stylesheet for 466x466 round display */
        ESP_UTILS_CHECK_FALSE_EXIT(phone->addStylesheet(STYLESHEET_466_466_ROUND_DARK), "Add stylesheet failed");
        ESP_UTILS_CHECK_FALSE_EXIT(phone->activateStylesheet(STYLESHEET_466_466_ROUND_DARK), "Activate stylesheet failed");

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