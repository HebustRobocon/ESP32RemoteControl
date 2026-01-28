#include "mainpage.h"
#include "lvgl/lvgl.h"

void main_page_create(void *user_data);
UI_PAGE_REGISTER("main_page", main_page_create);
static uint8_t main_page_created_flag = 0;
static char battery_show_str[24];

static void btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);
    if (code == LV_EVENT_CLICKED)
        sys_shutdown();
}

static void battery_voltage_show_cb(lv_timer_t *timer)
{
    lv_obj_t * label=( lv_obj_t *)lv_timer_get_user_data(timer);
    sprintf(battery_show_str,"Battery voltage:%.2f%%",Get_Battery_level(get_battery_voltage()));
    lv_label_set_text_static(label, battery_show_str);
    if(get_battery_voltage()<3.6f)
        sys_shutdown();
}

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
    xSemaphoreGive(get_screen_mutex());
    _key=key;   //使用静态变量防止发送失败
    asyn_comm_send_pack_nak(&_key,0x66,sizeof(_key));
    printf("发送\r\n");
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

    lv_obj_t *mylabel = lv_label_create(lv_screen_active());
    lv_label_set_text(mylabel, "Main Page Started");
    lv_obj_align(mylabel, LV_ALIGN_TOP_MID, 0, 100);

    lv_obj_t *shutdown_button = lv_btn_create(lv_screen_active());
    lv_obj_set_size(shutdown_button, 80, 40);
    lv_obj_align(shutdown_button, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(shutdown_button, btn_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *shutdown_button_label = lv_label_create(shutdown_button);
    lv_label_set_text(shutdown_button_label, "close");
    lv_obj_align(shutdown_button_label, LV_ALIGN_CENTER, 0, 0);

    lv_timer_t *battery_voltage_show_timer=lv_timer_create(battery_voltage_show_cb,200,mylabel);
    lv_timer_enable(battery_voltage_show_timer);
}
