#include "lvgl_port_disp.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "ILI9341.h"
#include "FT6336.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lvgl.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

TaskHandle_t lvgl_fresh_task_handler;
lv_display_t *display_screen;
lv_indev_t * indev_touch;

static uint16_t screen_buffer1[TFT_WIDTH*TFT_HEIGHT/6];
static uint16_t screen_buffer2[TFT_WIDTH*TFT_HEIGHT/6];

/* 刷新函数 */
void sceenflush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    if(disp==display_screen)
    {
        wait_spi_trans_finished();
        color_data_conversion(area->x1,area->y1,area->x2,area->y2,(uint16_t*)px_map);       //进行高低字节转换，然后放到发送缓冲区
        ILI9341_FreshScreen(area->x1,area->y1,area->x2,area->y2);
    }
    lv_display_flush_ready(disp);
}

//Debug
void my_log_cb(lv_log_level_t level, const char * buf)
{
    printf("[LVGL] level:%d~~~%s", level,buf);  // 输出到控制台
}

uint32_t lvgl_get_time_ms_cb()
{
    return esp_timer_get_time()/1000;
}

void lvgl_screen_fresh_task(void *param)
{
    QueueHandle_t lv_update_mutex=(QueueHandle_t)param;
    TickType_t last_wake_time=xTaskGetTickCount();
    while(1)
    {
        xSemaphoreTake(lv_update_mutex,portMAX_DELAY);
        int64_t start_time=esp_timer_get_time();
        lv_timer_handler();
        xSemaphoreGive(lv_update_mutex);
        //printf("%.2f%%\n",((float)((esp_timer_get_time()-start_time)))/(SCREEN_FRESH_PERIOD_MS*1000)*100.0f);  //统计CPU使用率
        vTaskDelayUntil(&last_wake_time,pdMS_TO_TICKS(SCREEN_FRESH_PERIOD_MS));
    }
}

void lvgl_indev_update_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    FT6336_Touch_t touch_info;
    ft6336_update_touch(&touch_info);
    if(touch_info.touch_count == 0)
    {
        data->state = LV_INDEV_STATE_REL;
    }else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touch_info.x1;
        data->point.y = touch_info.y1;
    }
}

QueueHandle_t mylvgl_port_init()
{
    //debug
    lv_log_register_print_cb(my_log_cb);

    QueueHandle_t lv_update_mutex=xSemaphoreCreateMutex();
    xSemaphoreGive(lv_update_mutex);
    lv_tick_set_cb(lvgl_get_time_ms_cb);
    display_screen=lv_display_create(TFT_WIDTH, TFT_HEIGHT);   //创建显示器

    lv_display_set_buffers(display_screen,screen_buffer1,screen_buffer2,TFT_HEIGHT*TFT_WIDTH*2/6,LV_DISPLAY_RENDER_MODE_PARTIAL);   //为显示器注册缓冲区
    lv_display_set_flush_cb(display_screen,sceenflush_cb);   //设置显示器输出回调函数

    indev_touch = lv_indev_create();
    lv_indev_set_type(indev_touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touch,lvgl_indev_update_cb);

    xTaskCreate(lvgl_screen_fresh_task,"lvgl_fresh",4096*2,lv_update_mutex,20,&lvgl_fresh_task_handler);  //创建显示器刷新任务
    return lv_update_mutex;
}
