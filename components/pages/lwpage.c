#include "lwpage.h"
#include "lvgl.h"

void lw_page_create(void *user_data);

UI_PAGE_REGISTER("lwpage", lw_page_create);
static uint8_t lw_page_created_flag = 0;
static char lw_info_str[32];

//按钮事件回调
static void lw_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED)
    {
       page_switch("main_page", NULL, NULL);
    }
}

// 定时器回调，更新页面信息
static void settings_info_update_cb(lv_timer_t *timer)
{
    lv_obj_t * label = (lv_obj_t *)lv_timer_get_user_data(timer);
    sprintf(lw_info_str, "System tick: %lu", lv_tick_get());
    lv_label_set_text_static(label, lw_info_str);
}

void lw_page_create(void *user_data)
{
    if(lw_page_created_flag)
        return;
    lw_page_created_flag = 1;

    

    //创建标签显示信息
    lv_obj_t *info_label = lv_label_create(lv_screen_active());
    lv_label_set_text(info_label, "lw");
    lv_obj_align(info_label, LV_ALIGN_TOP_MID, 0, 20);

    //创建按钮
    lv_obj_t *action_button = lv_btn_create(lv_screen_active());
    lv_obj_set_size(action_button, 100, 50);
    lv_obj_align(action_button, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(action_button, lw_btn_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *btn_label = lv_label_create(action_button);
    lv_label_set_text(btn_label, "Do Action");
    lv_obj_align(btn_label, LV_ALIGN_CENTER, 0, 0);

    // 创建定时器更新信息
    lv_timer_t *info_timer = lv_timer_create(settings_info_update_cb, 500, info_label);
    lv_timer_enable(info_timer);
}
