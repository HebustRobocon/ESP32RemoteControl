#include "mainpage.h"
#include "lvgl/lvgl.h"
#include "math.h"

void main_page_create(void *user_data);
UI_PAGE_REGISTER("main_page", main_page_create);
static uint8_t main_page_created_flag = 0;
static char battery_show_str[24];
extern int battery_adc_raw_value;
static void battery_voltage_show_cb(lv_timer_t *timer)
{
    lv_obj_t * label=( lv_obj_t *)lv_timer_get_user_data(timer);
    sprintf(battery_show_str,"Battery voltage:%d",battery_adc_raw_value);
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

static void main_page_remote_state_flush_func(const int *rocker, const uint16_t key,void* user_data)
{
    static uint16_t _key;
    static int update_cnt=0;
    if((update_cnt++)%10)
        return;
    xSemaphoreTake(get_screen_mutex(),portMAX_DELAY);
    char out_str[8]={0};
    sprintf(out_str,"0x%X",key);
    lv_obj_t *_label=(lv_obj_t *)user_data;
    lv_label_set_text(_label, out_str);
    
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

    lv_obj_t *keys_state_label = lv_label_create(lv_screen_active());
    lv_label_set_text(keys_state_label, "key:");
    lv_obj_align(keys_state_label, LV_ALIGN_TOP_MID, 0, 0);
    set_remote_flush_func(main_page_remote_state_flush_func,keys_state_label);

    rocker_label = lv_label_create(lv_screen_active());
    lv_label_set_text(rocker_label, "Rocker: 0,0,0,0");
    lv_obj_align(rocker_label, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t *mylabel = lv_label_create(lv_screen_active());
    lv_label_set_text(mylabel, "Main Page Started");
    lv_obj_align(mylabel, LV_ALIGN_TOP_MID, 0, 100);

    lv_timer_t *battery_voltage_show_timer=lv_timer_create(battery_voltage_show_cb,200,mylabel);
    lv_timer_enable(battery_voltage_show_timer);
}
