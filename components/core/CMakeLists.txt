idf_component_register(
    SRCS "core.c" "comm.c" "mylist.c" "data_poll.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos esp_timer
)