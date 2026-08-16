/*
 * SPDX-FileCopyrightText: 2024-2025 Custom
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/widgets/status_bar/esp_brookesia_status_bar.hpp"
#include "systems/phone/assets/esp_brookesia_phone_assets.h"

namespace esp_brookesia::systems::phone {

/**
 * 圆形屏幕状态栏边距计算：
 * 466x466 圆形屏幕，圆心 (233, 233)，半径 233
 * 状态栏高度 32px，中心位置 y=16
 * 距离圆心: d = 233 - 16 = 217
 * 可用半宽: sqrt(233² - 217²) = sqrt(54289 - 47089) = sqrt(7200) ≈ 85
 * 左右各需要裁剪: (233 - 85) = 148 像素
 * 实际设置 start_offset = 150 确保内容完全在可见区域内
 */
constexpr StatusBar::AreaData STYLESHEET_466_466_DARK_STATUS_BAR_AREA_DATA(int w_percent, StatusBar::AreaAlign align)
{
    return {
        .size = gui::StyleSize::RECT_PERCENT(w_percent, 100),
        .layout_column_align = align,
        .layout_column_start_offset = 150,  // 圆形屏幕需要更大的边距
        .layout_column_pad = 4,
    };
}

constexpr StatusBar::Data STYLESHEET_466_466_DARK_STATUS_BAR_DATA = {
    .main = {
        .size = gui::StyleSize::RECT_W_PERCENT(100, 32),  // 稍微降低高度
        .background_color = gui::StyleColor::COLOR(0x000000),  // 纯黑背景融入圆形边缘
        .text_font = gui::StyleFont::SIZE(14),  // 稍小的字体
        .text_color = gui::StyleColor::COLOR(0xFFFFFF),
    },
    .area = {
        .num = 2,
        .data = {
            STYLESHEET_466_466_DARK_STATUS_BAR_AREA_DATA(50, StatusBar::AreaAlign::START),
            STYLESHEET_466_466_DARK_STATUS_BAR_AREA_DATA(50, StatusBar::AreaAlign::END),
        },
    },
    .icon_common_size = gui::StyleSize::SQUARE(18),  // 稍小的图标
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
        .enable_battery_label = 0,  // 圆形屏幕空间有限，不显示电量百分比文字
        .enable_wifi_icon = 1,
        .enable_wifi_icon_common_size = 1,
        .enable_clock = 1,
    },
};

} // namespace esp_brookesia::systems::phone
