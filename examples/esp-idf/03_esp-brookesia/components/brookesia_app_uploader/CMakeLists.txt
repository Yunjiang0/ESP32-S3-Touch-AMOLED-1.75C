file(GLOB_RECURSE SRCS "*.c" "*.cpp")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "." "assets"
    REQUIRES brookesia_core esp_wifi esp_http_server esp_netif joltwallet__littlefs
)

# Force link to ensure app is registered
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u esp_brookesia_app_uploader_force_link")
