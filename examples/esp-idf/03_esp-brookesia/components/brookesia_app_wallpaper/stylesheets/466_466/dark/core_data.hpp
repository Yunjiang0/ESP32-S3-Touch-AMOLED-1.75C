/*
 * SPDX-FileCopyrightText: 2024-2025 Custom
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/base/esp_brookesia_base_context.hpp"

namespace esp_brookesia::systems::phone {

constexpr base::Display::Data STYLESHEET_466_466_DARK_CORE_DISPLAY_DATA = {
    .background = {
        .color = gui::StyleColor::COLOR(0x000000),
    },
    .text = {
        .default_fonts_num = 0,
    },
    .container = {},
};

constexpr base::Manager::Data STYLESHEET_466_466_DARK_CORE_MANAGER_DATA = {
    .app = {
        .max_running_num = 3,
    },
    .flags = {
        .enable_app_save_snapshot = 1,
    },
};

constexpr const char *STYLESHEET_466_466_DARK_CORE_INFO_DATA_NAME = "466x466 Round Dark";

constexpr base::Context::Data STYLESHEET_466_466_DARK_CORE_DATA = {
    .name = STYLESHEET_466_466_DARK_CORE_INFO_DATA_NAME,
    .screen_size = gui::StyleSize::RECT(466, 466),
    .display = STYLESHEET_466_466_DARK_CORE_DISPLAY_DATA,
    .manager = STYLESHEET_466_466_DARK_CORE_MANAGER_DATA,
};

} // namespace esp_brookesia::systems::phone
