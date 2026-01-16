idf_component_register(
    SRCS "lvgl_port_disp.c" "FT6336.c" "ILI9341.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_driver_spi lvgl freertos fatfs core
)