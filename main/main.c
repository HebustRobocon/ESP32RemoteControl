#include <stdint.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "ILI9341.h"
#include "FT6336.h"
#include "comm.h"
#include "core.h"
#include "lvgl_port_disp.h"
#include "page_manager.h"

#include "lvgl/lvgl.h"
#include "lvgl.h"

void app_main(void)
{
    sys_setup();
    //TODO:NVS初始化

    //屏幕显示初始化
    ILI9341_Init();
    FT6636_init();
    lv_init();
    QueueHandle_t screen_mutex=mylvgl_port_init();
    page_manager_init("main_page",screen_mutex);
    

    while (1)
    {
        printf("running...\r\n");
        vTaskDelay(5000);
    }
}
