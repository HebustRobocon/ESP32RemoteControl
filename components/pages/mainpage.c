#include "mainpage.h"
#include "lvgl/lvgl.h"
#include "math.h"
#include "lvgl/lv_conf.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "core.h"

void main_page_create(void *user_data);
UI_PAGE_REGISTER("main_page", main_page_create);
static uint8_t main_page_created_flag = 0;
static char battery_show_str[24];
extern int battery_adc_raw_value;
static void battery_voltage_show_cb(lv_timer_t *timer)
{
    lv_obj_t * label=( lv_obj_t *)lv_timer_get_user_data(timer);
    sprintf(battery_show_str,"Battery level:%.3f%%",Get_Battery_level(CalcBatteryVoltage()));
    lv_label_set_text_static(label, battery_show_str);
}

static lv_obj_t *rocker_label;
static lv_obj_t *battery_label;
static lv_obj_t *keys_state_label;
static int remote_rocker[4] = {0};
static uint16_t remote_key = 0;
static SemaphoreHandle_t remote_data_semaphore = NULL;
static volatile uint8_t labels_created = 0;

static void remote_state_task(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t update_interval = pdMS_TO_TICKS(20); // 20ms更新间隔
    
    while(1)
    {
        // 等待新的远程数据或超时
        if(remote_data_semaphore != NULL)
        {
            xSemaphoreTake(remote_data_semaphore, update_interval);
        }
        else
        {
            // 信号量未创建，直接延迟
            vTaskDelay(update_interval);
            continue;
        }
        
        // 只有当标签对象创建后才更新UI
        if(labels_created)
        {
            // 无论是否有新数据，都按固定频率处理
            xSemaphoreTake(get_screen_mutex(),portMAX_DELAY);
            char out_str[8]={0};
            sprintf(out_str,"0x%X",remote_key);
            sprintf(battery_show_str,"Battery voltage:%.3f",CalcBatteryVoltage());
            lv_label_set_text(keys_state_label, out_str);
            lv_label_set_text(battery_label, battery_show_str);

            // 直接使用处理后的值，不需要再计算
            int *rocker_processed = (int *)remote_rocker;
            
            char rocker_str[64]={0};
            sprintf(rocker_str,"Rocker: %d,%d,%d,%d",rocker_processed[0],rocker_processed[1],rocker_processed[2],rocker_processed[3]);
            lv_label_set_text(rocker_label, rocker_str);
            static PackControl_t remoteInfo;
            remoteInfo.rocker[0] = rocker_processed[0];
            remoteInfo.rocker[1] = rocker_processed[1];
            remoteInfo.rocker[2] = rocker_processed[2];
            remoteInfo.rocker[3] = rocker_processed[3];
            remoteInfo.Key = remote_key;
            xSemaphoreGive(get_screen_mutex());
            asyn_comm_send_pack_nak((uint8_t *)&remoteInfo,0x01,sizeof(remoteInfo));
        }
        
        // 使用vTaskDelayUntil确保固定频率
        vTaskDelayUntil(&last_wake_time, update_interval);
    }
}

static void main_page_remote_state_flush_func(const int *rocker, const uint16_t key,void* user_data)
{
    // 直接使用传递过来的处理后的值，不需要再计算
    // 复制数据到全局变量
    memcpy(remote_rocker, rocker, sizeof(remote_rocker));
    remote_key = key;
    
    // 通知远程状态任务有新数据
    if(remote_data_semaphore != NULL)
    {
        xSemaphoreGive(remote_data_semaphore);
    }
}

void main_page_create(void *user_data)
{
    if (main_page_created_flag)
        return;
    main_page_created_flag = 1;

    // 创建信号量用于远程数据同步
    remote_data_semaphore = xSemaphoreCreateBinary();
    if(remote_data_semaphore == NULL)
    {
        // 处理信号量创建失败
        printf("Failed to create remote data semaphore\n");
    }
    
    // 创建远程状态任务，优先级低于CoreTask
    BaseType_t task_create_result = xTaskCreate(remote_state_task, "remote_state_task", 4096, NULL, 4, NULL);
    if(task_create_result != pdPASS)
    {
        // 处理任务创建失败
        printf("Failed to create remote state task\n");
    }

    //通信模块初始化
    RemoteCommInit(NULL);
    //硬件状态更新任务初始化
    RemoteCoreInit();

    keys_state_label = lv_label_create(lv_screen_active());
    lv_label_set_text(keys_state_label, "key:");
    lv_obj_align(keys_state_label, LV_ALIGN_TOP_MID, 0, 0);
    set_remote_flush_func(main_page_remote_state_flush_func,keys_state_label);

    rocker_label = lv_label_create(lv_screen_active());
    lv_label_set_text(rocker_label, "Rocker: 0,0,0,0");
    lv_obj_align(rocker_label, LV_ALIGN_TOP_MID, 0, 50);

    battery_label = lv_label_create(lv_screen_active());
    lv_label_set_text(battery_label, "Battery voltage:");
    lv_obj_align(battery_label, LV_ALIGN_TOP_MID, 0, 150);
    set_remote_flush_func(main_page_remote_state_flush_func,battery_label);
    
    // 所有标签创建完成，设置标志
    labels_created = 1;

    lv_obj_t *L_label = lv_label_create(lv_screen_active());
    lv_label_set_text(L_label, "L");
    // 创建样式
    static lv_style_t style_L;
    lv_style_init(&style_L);
    lv_style_set_text_color(&style_L, lv_color_hex(0xFF0000));//蓝色
    lv_style_set_text_font(&style_L, &lv_font_montserrat_48);
    // 应用样式
    lv_obj_add_style(L_label, &style_L, 0);
    lv_obj_align(L_label, LV_ALIGN_TOP_MID, -80, 200);

    lv_obj_t *M_label = lv_label_create(lv_screen_active());
    lv_label_set_text(M_label, "M");
    // 创建样式
    static lv_style_t style_M;
    lv_style_init(&style_M);
    lv_style_set_text_color(&style_M, lv_color_hex(0x003F34));//粉色
    lv_style_set_text_font(&style_M, &lv_font_montserrat_48);
    // 应用样式
    lv_obj_add_style(M_label, &style_M, 0);
    lv_obj_align(M_label, LV_ALIGN_TOP_MID, 80, 200);

    lv_obj_t *z_label = lv_label_create(lv_screen_active()); 
    // 使用UTF-8编码的字符串，避免中文乱码
    lv_label_set_text(z_label, " 第 一 !!!");
    // 创建样式
    static lv_style_t style_Z;
    lv_style_init(&style_Z);
    lv_style_set_text_color(&style_Z, lv_color_hex(0xFF00FF));
    // 使用支持中文的SimSun字体
    lv_style_set_text_font(&style_Z, &lv_font_simsun_16_cjk);
    // 应用样式
    lv_obj_add_style(z_label, &style_Z, 0);
    lv_obj_align(z_label, LV_ALIGN_TOP_MID, 0, 300);

    lv_obj_t *mylabel = lv_label_create(lv_screen_active());
    lv_timer_t *battery_voltage_show_timer=lv_timer_create(battery_voltage_show_cb,200,mylabel);
    lv_timer_enable(battery_voltage_show_timer);
    lv_obj_align(mylabel, LV_ALIGN_TOP_MID, 0, 100);
}