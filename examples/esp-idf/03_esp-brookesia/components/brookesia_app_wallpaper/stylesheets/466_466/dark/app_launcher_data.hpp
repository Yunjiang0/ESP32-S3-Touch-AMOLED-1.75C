/*
 * SPDX-FileCopyrightText: 2024-2025 Custom
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/widgets/app_launcher/esp_brookesia_app_launcher.hpp"

namespace esp_brookesia::systems::phone {

constexpr AppLauncherIcon::Data STYLESHEET_466_466_DARK_APP_LAUNCHER_ICON_DATA = {
    .main = {
        .size = gui::StyleSize::SQUARE(160),  // 缩小以适应圆形屏幕
        .layout_row_pad = 8,
    },
    .image = {
        .default_size = gui::StyleSize::SQUARE(100),
        .press_size = gui::StyleSize::SQUARE(90),
    },
    .label = {
        .text_font = gui::StyleFont::SIZE(18),
        .text_color = gui::StyleColor::COLOR(0xFFFFFF),
    }
};

constexpr AppLauncherData STYLESHEET_466_466_DARK_APP_LAUNCHER_DATA = {
    .main = {
        .y_start = 0,
        .size = gui::StyleSize::RECT_PERCENT(100, 100),
    },
    .table = {
        .default_num = 2,  // 2x2 布局适合圆形屏幕
        .size = gui::StyleSize::RECT_W_PERCENT(100, 340),
    },
    .indicator = {
        .main_size = gui::StyleSize::RECT_W_PERCENT(100, 60),
        .main_layout_column_pad = 8,
        .main_layout_bottom_offset = 20,  // 增加底部边距
        .spot_inactive_size = gui::StyleSize::SQUARE(10),
        .spot_active_size = gui::StyleSize::RECT(24, 10),
        .spot_inactive_background_color = gui::StyleColor::COLOR(0xC6C6C6),
        .spot_active_background_color = gui::StyleColor::COLOR(0xFFFFFF),
    },
    .icon = STYLESHEET_466_466_DARK_APP_LAUNCHER_ICON_DATA,
    .flags = {
        .enable_table_scroll_anim = 0,
    },
};

} // namespace esp_brookesia::systems::phone
