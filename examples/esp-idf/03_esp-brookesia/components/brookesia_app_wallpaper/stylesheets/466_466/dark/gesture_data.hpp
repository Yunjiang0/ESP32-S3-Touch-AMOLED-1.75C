/*
 * SPDX-FileCopyrightText: 2024-2025 Custom
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/widgets/gesture/esp_brookesia_gesture.hpp"

namespace esp_brookesia::systems::phone {

constexpr Gesture::IndicatorBarData STYLESHEET_466_466_DARK_GESTURE_LEFT_RIGHT_INDICATOR_BAR_DATA = {
    .main = {
        .size_min = gui::StyleSize::RECT(8, 0),
        .size_max = gui::StyleSize::RECT_H_PERCENT(8, 50),
        .radius = 4,
        .layout_pad_all = 2,
        .color = gui::StyleColor::COLOR(0x000000),
    },
    .indicator = {
        .radius = 4,
        .color = gui::StyleColor::COLOR(0xFFFFFF),
    },
    .animation = {
        .scale_back_path_type = gui::StyleAnimation::ANIM_PATH_TYPE_BOUNCE,
        .scale_back_time_ms = 500,
    },
};

constexpr Gesture::IndicatorBarData STYLESHEET_466_466_DARK_GESTURE_BOTTOM_INDICATOR_BAR_DATA = {
    .main = {
        .size_min = gui::StyleSize::RECT(0, 8),
        .size_max = gui::StyleSize::RECT_W_PERCENT(50, 8),
        .radius = 4,
        .layout_pad_all = 2,
        .color = gui::StyleColor::COLOR(0x1A1A1A),
    },
    .indicator = {
        .radius = 4,
        .color = gui::StyleColor::COLOR(0xFFFFFF),
    },
    .animation = {
        .scale_back_path_type = gui::StyleAnimation::ANIM_PATH_TYPE_BOUNCE,
        .scale_back_time_ms = 500,
    },
};

constexpr Gesture::Data STYLESHEET_466_466_DARK_GESTURE_DATA = {
    .detect_period_ms = 20,
    .threshold = {
        .direction_vertical = 50,
        .direction_horizon = 50,
        .direction_angle = 60,
        .horizontal_edge = 10,
        .vertical_edge = 20,
        .duration_short_ms = 800,
        .speed_slow_px_per_ms = 0.1,
    },
    .indicator_bars = {
        [static_cast<int>(Gesture::IndicatorBarType::LEFT)] =
        STYLESHEET_466_466_DARK_GESTURE_LEFT_RIGHT_INDICATOR_BAR_DATA,
        [static_cast<int>(Gesture::IndicatorBarType::RIGHT)] =
        STYLESHEET_466_466_DARK_GESTURE_LEFT_RIGHT_INDICATOR_BAR_DATA,
        [static_cast<int>(Gesture::IndicatorBarType::BOTTOM)] =
        STYLESHEET_466_466_DARK_GESTURE_BOTTOM_INDICATOR_BAR_DATA,
    },
    .flags = {
        .enable_indicator_bars = {
            [static_cast<int>(Gesture::IndicatorBarType::LEFT)] = 0,
            [static_cast<int>(Gesture::IndicatorBarType::RIGHT)] = 0,
            [static_cast<int>(Gesture::IndicatorBarType::BOTTOM)] = 1,
        },
    },
};

} // namespace esp_brookesia::systems::phone
