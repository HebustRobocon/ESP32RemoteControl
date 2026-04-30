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
static char battery_percent_str[24];
extern int battery_adc_raw_value;

static lv_obj_t *battery_arc;
static lv_obj_t *battery_percent_label;
static lv_obj_t *rocker_x_label;
static lv_obj_t *rocker_y_label;
static lv_obj_t *rocker_x_arc;
static lv_obj_t *rocker_y_arc;
static lv_obj_t *keys_state_label;

static int remote_rocker[4] = {0};
static uint16_t remote_key = 0;
static SemaphoreHandle_t remote_data_semaphore = NULL;

// 全局任务句柄，供其他文件访问
TaskHandle_t remote_state_task_handle = NULL;

static void battery_update_cb(lv_timer_t *timer)
{
    float voltage = CalcBatteryVoltage();
    float level = Get_Battery_level(voltage);
    
    if(lv_obj_is_valid(battery_arc)) {
        lv_arc_set_value(battery_arc, (int32_t)level);
    }
    if(lv_obj_is_valid(battery_percent_label)) {
        sprintf(battery_percent_str, "%.0f%%", level);
        lv_label_set_text(battery_percent_label, battery_percent_str);
    }
}

static void remote_state_task(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t update_interval = pdMS_TO_TICKS(20);
    
    while(1)
    {
        if(remote_data_semaphore != NULL)
        {
            xSemaphoreTake(remote_data_semaphore, update_interval);
        }
        else
        {
            vTaskDelay(update_interval);
            continue;
        }
        
        lv_obj_t *scr = lv_screen_active();
        if(scr == NULL)
        {
            vTaskDelay(update_interval);
            continue;
        }
        
        if(!lv_obj_is_valid(keys_state_label) || !lv_obj_is_valid(rocker_x_label) || !lv_obj_is_valid(rocker_y_label))
        {
            vTaskDelay(update_interval);
            continue;
        }
        
        xSemaphoreTake(get_screen_mutex(),portMAX_DELAY);
        if(lv_obj_is_valid(keys_state_label) && lv_obj_is_valid(rocker_x_label) && lv_obj_is_valid(rocker_y_label))
        {
            char out_str[16]={0};
            sprintf(out_str,"KEY: 0x%04X", remote_key);
            lv_label_set_text(keys_state_label, out_str);
            
            int *rocker_processed = (int *)remote_rocker;
            
            char x_str[16], y_str[16];
            sprintf(x_str, "X: %d", rocker_processed[0]);
            sprintf(y_str, "Y: %d", rocker_processed[1]);
            lv_label_set_text(rocker_x_label, x_str);
            lv_label_set_text(rocker_y_label, y_str);
            
            // Normalize rocker values (-100 to 100) for arc display
            int32_t x_val = (rocker_processed[0] + 100) / 2;
            int32_t y_val = (rocker_processed[1] + 100) / 2;
            if(lv_obj_is_valid(rocker_x_arc)) lv_arc_set_value(rocker_x_arc, x_val);
            if(lv_obj_is_valid(rocker_y_arc)) lv_arc_set_value(rocker_y_arc, y_val);
        }
        xSemaphoreGive(get_screen_mutex());
        
        static PackControl_t remoteInfo;
        remoteInfo.rocker[0] = remote_rocker[0];
        remoteInfo.rocker[1] = remote_rocker[1];
        remoteInfo.rocker[2] = remote_rocker[2];
        remoteInfo.rocker[3] = remote_rocker[3];
        remoteInfo.Key = remote_key;
        asyn_comm_send_pack_nak((uint8_t *)&remoteInfo,0x01,sizeof(remoteInfo));
    
        vTaskDelayUntil(&last_wake_time, update_interval);
    }
}

static void lwpage_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED)
    {
        if(remote_state_task_handle != NULL)
        {
            vTaskSuspend(remote_state_task_handle);
        }
        page_switch("lw_page", NULL, NULL);
    }
}

static void main_page_remote_state_flush_func(const int *rocker, const uint16_t key,void* user_data)
{
    memcpy(remote_rocker, rocker, sizeof(remote_rocker));
    remote_key = key;
    
    if(remote_data_semaphore != NULL)
    {
        xSemaphoreGive(remote_data_semaphore);
    }
}

void main_page_create(void *user_data)
{
    if (main_page_created_flag)
    {
        return;
    }
    main_page_created_flag = 1;

    remote_data_semaphore = xSemaphoreCreateBinary();
    xTaskCreate(remote_state_task, "remote_state_task", 4096, NULL, 4, &remote_state_task_handle);

    RemoteCommInit(NULL);
    RemoteCoreInit();

    // Set screen background
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x121212), LV_PART_MAIN);
    
    // Create header container
    lv_obj_t *header_cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(header_cont, 320, 60);
    lv_obj_align(header_cont, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header_cont, lv_color_hex(0x1E1E1E), LV_PART_MAIN);
    lv_obj_set_style_border_width(header_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header_cont, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(header_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Title
    lv_obj_t *title_label = lv_label_create(header_cont);
    lv_label_set_text(title_label, "ROBOCON RC");
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x00E5FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, LV_PART_MAIN);
    
    // Battery container
    lv_obj_t *battery_cont = lv_obj_create(header_cont);
    lv_obj_set_size(battery_cont, 80, 50);
    lv_obj_set_style_bg_opa(battery_cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(battery_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(battery_cont, 0, LV_PART_MAIN);
    
    battery_arc = lv_arc_create(battery_cont);
    lv_arc_set_range(battery_arc, 0, 100);
    lv_arc_set_value(battery_arc, 0);
    lv_obj_set_size(battery_arc, 40, 40);
    lv_obj_center(battery_arc);
    lv_obj_set_style_arc_color(battery_arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(battery_arc, lv_color_hex(0x00E676), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(battery_arc, 4, LV_PART_MAIN | LV_PART_INDICATOR);
    
    battery_percent_label = lv_label_create(battery_cont);
    lv_label_set_text(battery_percent_label, "0%");
    lv_obj_center(battery_percent_label);
    lv_obj_set_style_text_color(battery_percent_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(battery_percent_label, &lv_font_montserrat_10, LV_PART_MAIN);
    
    // Create main content container
    lv_obj_t *main_cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(main_cont, 320, 260);
    lv_obj_align(main_cont, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_set_style_bg_opa(main_cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(main_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(main_cont, 15, LV_PART_MAIN);
    
    // Create rocker cards
    lv_obj_t *rocker_card = lv_obj_create(main_cont);
    lv_obj_set_size(rocker_card, 290, 140);
    lv_obj_align(rocker_card, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(rocker_card, lv_color_hex(0x1E1E1E), LV_PART_MAIN);
    lv_obj_set_style_radius(rocker_card, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(rocker_card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(rocker_card, 12, LV_PART_MAIN);
    
    // Rocker title
    lv_obj_t *rocker_title = lv_label_create(rocker_card);
    lv_label_set_text(rocker_title, "ROCKER STATE");
    lv_obj_align(rocker_title, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(rocker_title, lv_color_hex(0xBBBBBB), LV_PART_MAIN);
    lv_obj_set_style_text_font(rocker_title, &lv_font_montserrat_12, LV_PART_MAIN);
    
    // X axis container
    lv_obj_t *x_cont = lv_obj_create(rocker_card);
    lv_obj_set_size(x_cont, 260, 40);
    lv_obj_align(x_cont, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_opa(x_cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(x_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(x_cont, 0, LV_PART_MAIN);
    
    rocker_x_arc = lv_arc_create(x_cont);
    lv_arc_set_mode(rocker_x_arc, LV_ARC_MODE_SYMMETRICAL);
    lv_arc_set_range(rocker_x_arc, 0, 100);
    lv_arc_set_value(rocker_x_arc, 50);
    lv_obj_set_size(rocker_x_arc, 30, 30);
    lv_obj_align(rocker_x_arc, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_arc_color(rocker_x_arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(rocker_x_arc, lv_color_hex(0xFF5252), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(rocker_x_arc, 4, LV_PART_MAIN | LV_PART_INDICATOR);
    
    rocker_x_label = lv_label_create(x_cont);
    lv_label_set_text(rocker_x_label, "X: 0");
    lv_obj_align(rocker_x_label, LV_ALIGN_LEFT_MID, 40, 0);
    lv_obj_set_style_text_color(rocker_x_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(rocker_x_label, &lv_font_montserrat_14, LV_PART_MAIN);
    
    // Y axis container
    lv_obj_t *y_cont = lv_obj_create(rocker_card);
    lv_obj_set_size(y_cont, 260, 40);
    lv_obj_align(y_cont, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_bg_opa(y_cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(y_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(y_cont, 0, LV_PART_MAIN);
    
    rocker_y_arc = lv_arc_create(y_cont);
    lv_arc_set_mode(rocker_y_arc, LV_ARC_MODE_SYMMETRICAL);
    lv_arc_set_range(rocker_y_arc, 0, 100);
    lv_arc_set_value(rocker_y_arc, 50);
    lv_obj_set_size(rocker_y_arc, 30, 30);
    lv_obj_align(rocker_y_arc, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_arc_color(rocker_y_arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(rocker_y_arc, lv_color_hex(0x5252FF), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(rocker_y_arc, 4, LV_PART_MAIN | LV_PART_INDICATOR);
    
    rocker_y_label = lv_label_create(y_cont);
    lv_label_set_text(rocker_y_label, "Y: 0");
    lv_obj_align(rocker_y_label, LV_ALIGN_LEFT_MID, 40, 0);
    lv_obj_set_style_text_color(rocker_y_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(rocker_y_label, &lv_font_montserrat_14, LV_PART_MAIN);
    
    // Keys card
    lv_obj_t *keys_card = lv_obj_create(main_cont);
    lv_obj_set_size(keys_card, 290, 60);
    lv_obj_align(keys_card, LV_ALIGN_TOP_MID, 0, 160);
    lv_obj_set_style_bg_color(keys_card, lv_color_hex(0x1E1E1E), LV_PART_MAIN);
    lv_obj_set_style_radius(keys_card, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(keys_card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(keys_card, 12, LV_PART_MAIN);
    
    keys_state_label = lv_label_create(keys_card);
    lv_label_set_text(keys_state_label, "KEY: 0x0000");
    lv_obj_center(keys_state_label);
    lv_obj_set_style_text_color(keys_state_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(keys_state_label, &lv_font_montserrat_16, LV_PART_MAIN);
    
    set_remote_flush_func(main_page_remote_state_flush_func, keys_state_label);
    
    // LW Page button
    lv_obj_t *lwpage_button = lv_btn_create(lv_screen_active());
    lv_obj_set_size(lwpage_button, 200, 50);
    lv_obj_align(lwpage_button, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_color(lwpage_button, lv_color_hex(0x00E5FF), LV_PART_MAIN);
    lv_obj_set_style_bg_color(lwpage_button, lv_color_hex(0x00B8D4), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(lwpage_button, 25, LV_PART_MAIN);
    lv_obj_add_event_cb(lwpage_button, lwpage_btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *lwpage_btn_label = lv_label_create(lwpage_button);
    lv_label_set_text(lwpage_btn_label, "LW PAGE");
    lv_obj_center(lwpage_btn_label);
    lv_obj_set_style_text_color(lwpage_btn_label, lv_color_hex(0x121212), LV_PART_MAIN);
    lv_obj_set_style_text_font(lwpage_btn_label, &lv_font_montserrat_16, LV_PART_MAIN);
    
    lv_timer_t *battery_timer = lv_timer_create(battery_update_cb, 500, NULL);
    lv_timer_enable(battery_timer);
    
    if(remote_state_task_handle != NULL)
    {
        vTaskResume(remote_state_task_handle);
    }
}