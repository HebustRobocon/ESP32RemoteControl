#ifndef __LVGL_PORT_DISP_H_
#define __LVGL_PORT_DISP_H_

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define SCREEN_FRESH_PERIOD_MS  50  
#define LVGL_TICK_PERIOD_MS     5


QueueHandle_t mylvgl_port_init();

#endif
