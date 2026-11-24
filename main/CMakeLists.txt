idf_component_register(
    SRCS
        "main.cpp"
        "apps/db_base_app.cpp"
        "services/brookesia_storage.cpp"
        "services/display_service.cpp"
        "services/storage_service.cpp"
        "services/clock_service.cpp"
        "services/wifi_time_service.cpp"
        "services/power_service.cpp"
        "services/button_service.cpp"
        "services/motion_service.cpp"
        "services/sound_service.cpp"
        "ui/i18n/db_i18n.cpp"
        "ui/theme/db_theme.cpp"
        "ui/home/db_desktop.cpp"
        "ui/home/db_power_overlay.cpp"
        "ui/home/db_home_header.cpp"
        "ui/assets/db_icons.c"
        "ui/assets/db_ui_icons.c"
        "ui/assets/db_woodfish_art.c"
        "ui/assets/db_font_16.c"
        "ui/assets/db_font_20.c"
        "ui/assets/db_font_26.c"
        "ui/assets/db_font_36.c"
        "ui/assets/db_font_hero_48.c"
        "apps/random_lab/random_lab_app.cpp"
        "apps/wooden_fish/wooden_fish_app.cpp"
        "apps/daily_fortune/daily_fortune_app.cpp"
        "apps/game_2048/game_2048_app.cpp"
        "apps/relative_calculator/relative_calc_app.cpp"
        "apps/time_tools/time_tools_app.cpp"
        "apps/settings/settings_app.cpp"
        "apps/settings/settings_time.cpp"
    INCLUDE_DIRS "."
    EMBED_TXTFILES "services/wifi_time_portal.html"
    REQUIRES
        nvs_flash
        esp_timer
        driver
        desk_buddy_core
        esp_wifi
        esp_netif
        esp_event
        esp_http_server
        lwip
)

target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-missing-field-initializers)

if(DESK_BUDDY_DEVICE_SMOKE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE DESK_BUDDY_DEVICE_SMOKE=1)
endif()

function(get_component_library component_name output_var)
    idf_build_get_property(build_components BUILD_COMPONENTS)
    set(TARGET_COMPONENT "")
    foreach(COMPONENT ${build_components})
        if(COMPONENT MATCHES "${component_name}" OR COMPONENT MATCHES "espressif__${component_name}")
            set(TARGET_COMPONENT ${COMPONENT})
            break()
        endif()
    endforeach()
    if(TARGET_COMPONENT STREQUAL "")
        message(FATAL_ERROR "Component '${component_name}' not found.")
    else()
        idf_component_get_property(COMPONENT_LIB ${TARGET_COMPONENT} COMPONENT_LIB)
        set(${output_var} ${COMPONENT_LIB} PARENT_SCOPE)
    endif()
endfunction()

get_component_library("lvgl" LVGL_LIB)
target_compile_options(${LVGL_LIB} PUBLIC "-Wno-attributes")

# lwIP's 15-second reply timeout exceeds the 10-second sync session, preventing server fallback.
idf_component_get_property(LWIP_LIB lwip COMPONENT_LIB)
target_compile_definitions(${LWIP_LIB} PRIVATE SNTP_RECV_TIMEOUT=3000)
