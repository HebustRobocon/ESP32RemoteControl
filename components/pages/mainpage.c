#include "mainpage.h"
#include "lvgl/lvgl.h"
#include "math.h"
#include "lvgl/lv_conf.h"

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

int ProcessRocker(int adc_value, int dead_zone, int offset)
{
    int v = adc_value + offset; // 注意这里是 + offset
    int center = 0x7FF;         // 恒定中心点 2047

    // 死区
    if (v < center + dead_zone && v > center - dead_zone)
        return 0;

    // 计算偏移值，不进行归一化
    int out;
    if (v > center)
        out = v - center - dead_zone;
    else
        out = v - center + dead_zone;

    // 反转方向（加负号）
    return -out;
}

static lv_obj_t *rocker_label;
static lv_obj_t *battery_label;
static lv_obj_t *keys_state_label;
static void main_page_remote_state_flush_func(const int *rocker, const uint16_t key,void* user_data)
{
    static int update_cnt=0;
    if((update_cnt++)%10)
        return;
    xSemaphoreTake(get_screen_mutex(),portMAX_DELAY);
    char out_str[8]={0};
    sprintf(out_str,"0x%X",key);
    sprintf(battery_show_str,"Battery voltage:%.3f",CalcBatteryVoltage());
    lv_label_set_text(keys_state_label, out_str);
    lv_label_set_text(battery_label, battery_show_str);

    int rocker_processed[4];
    rocker_processed[0] = ProcessRocker(rocker[0], 250, 0);
    rocker_processed[1] = ProcessRocker(rocker[1], 200, 0);
    rocker_processed[2] = ProcessRocker(rocker[2], 200, 0);
    rocker_processed[3] = ProcessRocker(rocker[3], 200, 0);
    
    char rocker_str[32]={0};
    sprintf(rocker_str,"Rocker: %d,%d,%d,%d",rocker_processed[0],rocker_processed[1],rocker_processed[2],rocker_processed[3]);
    lv_label_set_text(rocker_label, rocker_str);
    static PackControl_t remoteInfo;
    remoteInfo.rocker[0] = rocker_processed[0];
    remoteInfo.rocker[1] = rocker_processed[1];
    remoteInfo.rocker[2] = rocker_processed[2];
    remoteInfo.rocker[3] = rocker_processed[3];
    remoteInfo.Key = key;
    xSemaphoreGive(get_screen_mutex());
    asyn_comm_send_pack_nak((uint8_t *)&remoteInfo,0x01,sizeof(remoteInfo));
    printf("1\r\n");
}

void main_page_create(void *user_data)
{
    if (main_page_created_flag)
        return;
    main_page_created_flag = 1;

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