#include "lwpage.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern TaskHandle_t remote_state_task_handle;

void lw_page_create(void *user_data);
UI_PAGE_REGISTER("lw_page", lw_page_create);

// 延迟返回上一页面，避免在事件回调中直接删除当前屏幕
static void lw_return_last_page_timer_cb(lv_timer_t *timer)
{
    lv_timer_del(timer);
    return_last_page();
}

//按钮事件回调
static void lw_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED)
    {
        if(remote_state_task_handle != NULL)
        {
            vTaskResume(remote_state_task_handle);
        }
        lv_timer_t *timer = lv_timer_create(lw_return_last_page_timer_cb, 1, NULL);
        if(timer) {
            lv_timer_set_repeat_count(timer, 1);
        }
    }
}

void lw_page_create(void *user_data)
{
    //创建标签显示信息
    lv_obj_t *info_label = lv_label_create(lv_screen_active());
    lv_label_set_text(info_label, "LW Page");
    lv_obj_align(info_label, LV_ALIGN_TOP_MID, 0, 50);

    //创建按钮
    lv_obj_t *action_button = lv_btn_create(lv_screen_active());
    lv_obj_set_size(action_button, 120, 60);
    lv_obj_align(action_button, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(action_button, lw_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(action_button);
    lv_label_set_text(btn_label, "Back");
    lv_obj_align(btn_label, LV_ALIGN_CENTER, 0, 0);
}
