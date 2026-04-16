#include "lwpage.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern TaskHandle_t remote_state_task_handle;

void lw_page_create(void *user_data);
UI_PAGE_REGISTER("lw_page", lw_page_create);
typedef enum
{
    STATE_NONE = 0, // 绿色 - 无
    STATE_R1 = 1,   // 黄色 - R1-KFS
    STATE_R2 = 2,   // 红色 - R2-KFS
    STATE_COUNT
} block_state_t;
static block_state_t block_states[12] = {STATE_NONE}; // 存储 12 个块的状态

// 延迟返回上一页面，避免在事件回调中直接删除当前屏幕
static void lw_return_last_page_timer_cb(lv_timer_t *timer)
{
    lv_timer_del(timer);
    return_last_page();
}

// 按钮事件回调
static void lw_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        if (remote_state_task_handle != NULL)
        {
            vTaskResume(remote_state_task_handle);
        }
        lv_timer_t *timer = lv_timer_create(lw_return_last_page_timer_cb, 1, NULL);
        if (timer)
        {
            lv_timer_set_repeat_count(timer, 1);
        }
    }
}
// 按钮点击回调函数
static void grid_btn_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);           // 获取按钮里的 label
    intptr_t index = (intptr_t)lv_event_get_user_data(e); // 获取是哪个按钮

    // 状态循环切换
    block_states[index] = (block_states[index] + 1) % STATE_COUNT;

    // 根据状态更新 UI
    switch (block_states[index])
    {
    case STATE_NONE:
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_label_set_text(label, "F");
        break;
    case STATE_R1:
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_YELLOW), 0);
        lv_label_set_text(label, "R1");
        break;
    case STATE_R2:
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text(label, "R2");
        break;
    default:
        break;
    }
}

void lw_page_create(void *user_data)
{
    // 3*4 UI页面
    lv_obj_t *cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(cont, 240, 250);            // 适配你的 240*360 屏幕，留点空间给标题
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 0); // 靠顶对齐

    // 设置 Flex 布局 (网格样式)
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont, 5, 0);
    lv_obj_set_style_pad_gap(cont, 5, 0);

    // 循环创建 12 个方块 (3 * 4)
    for (int i = 0; i < 12; i++)
    {
        lv_obj_t *btn = lv_button_create(cont);
        lv_obj_set_size(btn, 55, 55); // 宽度约等于 (240-间距)/3

        // 设置初始样式 (绿色)
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREEN), 0);

        // 创建文字
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, "F");
        lv_obj_center(label);

        // 绑定事件，并将索引 i 作为用户数据传递
        lv_obj_add_event_cb(btn, grid_btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    // 创建按钮
    lv_obj_t *action_button = lv_btn_create(lv_screen_active());
    lv_obj_set_size(action_button, 100, 60);
    lv_obj_align(action_button, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(action_button, lw_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(action_button);
    lv_label_set_text(btn_label, "Back");
    lv_obj_align(btn_label, LV_ALIGN_CENTER, 0, 0);
}
