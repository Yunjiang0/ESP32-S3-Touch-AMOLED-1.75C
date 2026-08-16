idf_component_register(
    SRCS "esp_brookesia_app_clock.cpp"
         "assets/esp_brookesia_app_icon_launcher_clock_112_112.c"
         "ui/ui_img_clock_hour_png.c"
         "ui/ui_img_clock_min_png.c"
         "ui/ui_img_clock_sec_png.c"
         "ui/ui_img_pattern_png.c"
         "ui/ui_font_Number.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES brookesia_core
)
