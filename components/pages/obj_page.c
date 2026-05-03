#include "obj_page.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mainpage.h"

#define PACK_LW_GRID_CMD 0x06

extern lv_obj_t *keys_state_label;
extern TaskHandle_t remote_state_task_handle;
void obj_page_create(void *user_data);
UI_PAGE_REGISTER("obj_page", obj_page_create);

typedef enum
{
    STATE_NONE = 0, // 绿色 - 无
    STATE_R1 = 1,   // 黄色 - R1-KFS
    STATE_R2 = 2,   // 红色 - R2-KFS
    STATE_COUNT     // 注:利用枚举自动递增的特性，STATE_COUNT 的数值正好等于前面所有有效状态的总数
} block_state_t;
static block_state_t block_states[12] = {STATE_NONE}; // 存储 12 个块的状态

static void lw_return_last_page_timer_cb(lv_timer_t *timer)
{
    lv_timer_del(timer);
    return_last_page();
}

// 修改后的点击回调函数
static void grid_btn_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    intptr_t index = (intptr_t)lv_event_get_user_data(e);

    // 1. 状态切换 (0 -> 1 -> 2 -> 0)
    block_states[index] = (block_states[index] + 1) % STATE_COUNT;

    // 2. 根据索引和状态更新 UI
    if (block_states[index] == STATE_NONE)
    {
        // 共有逻辑：白色 + "N"
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x000000), 0); // 白色
        lv_label_set_text(label, "N");
    }
    else if (index <= 2)
    {
        // 0, 1, 2 号方格逻辑
        if (block_states[index] == STATE_R1)
        {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x76300F), 0); // 深蓝色
            lv_label_set_text(label, "R1");
        }
        else
        {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x009CB8), 0); // 紫色
            lv_label_set_text(label, "R1");
        }
    }
    else
    {
        // 3 - 8 号方格逻辑
        if (block_states[index] == STATE_R1)
        {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x76300F), 0); // 深蓝色
            lv_label_set_text(label, "R2");
        }
        else
        {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x009CB8), 0); // 紫色
            lv_label_set_text(label, "R2");
        }
    }
}

static void back_to_main_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        set_remote_flush_func(main_page_remote_state_flush_func, keys_state_label); // 重新挂载回调函数
        if (remote_state_task_handle != NULL)
        {
            vTaskResume(remote_state_task_handle); // 恢复 UI 任务
        }
        lv_timer_t *timer = lv_timer_create(lw_return_last_page_timer_cb, 1, NULL);
        if (timer)
        {
            lv_timer_set_repeat_count(timer, 1);
        }
    }
}

void obj_page_create(void *user_data)
{
    // 1. 创建 3x3 矩阵容器
    lv_obj_t *cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(cont, 220, 220);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 20);
    // 设置Flex布局
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont, 5, 0);
    lv_obj_set_style_pad_gap(cont, 5, 0); // 设置按钮之间的间距

    // lv_obj_t *btn = lv_button_create(cont);
    // lv_obj_t *label = lv_label_create(btn);
    // lv_label_set_text(label, "OBJ");

    // 在 obj_page_create 的循环中修改状态恢复逻辑
    for (int i = 0; i < 9; i++)
    {
        int real_index = 8 - i; // 注意：如果是3x3，最大索引应为8

        lv_obj_t *btn = lv_button_create(cont);
        lv_obj_set_size(btn, 55, 55);
        lv_obj_t *label = lv_label_create(btn);

        // 初始状态判断逻辑
        if (block_states[real_index] == STATE_NONE)
        {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x000000), 0); // 初始全白(没有物块)
            lv_label_set_text(label, "N");
        }
        else if (real_index <= 2)
        {
            // 0-2 号恢复逻辑
            lv_label_set_text(label, "R1");
            lv_obj_set_style_bg_color(btn, (block_states[real_index] == STATE_R1) ? lv_color_hex(0x76300F) : lv_color_hex(0x009CB8), 0); // 浅海蓝 紫色
        }
        else
        {
            // 3-8 号恢复逻辑
            lv_label_set_text(label, "R2");
            lv_obj_set_style_bg_color(btn, (block_states[real_index] == STATE_R1) ? lv_color_hex(0x76300F) : lv_color_hex(0x009CB8), 0); // 浅海蓝 紫色
        }

        lv_obj_center(label);
        lv_obj_add_event_cb(btn, grid_btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)real_index);
    }

    // 2. 底部返回按钮
    lv_obj_t *back_btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(back_btn, back_to_main_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "RETURN");
    lv_obj_center(back_label);
}

// 青青草原绿 0xFF007F
// 暗绿色     0xFFC0CB
// 正红色     0x00FFFF
// 普鲁士蓝   0xFFCEAC(适合背景色__黑蓝色)
// 深海蓝     0xFF9B6B
// 午夜蓝     0xE6E68F
// 深蓝色     0xFFFF00
// 天蓝色     0x783114
// 道奇蓝     0xE16F00
// 浅海蓝     0x76300F
// 雾霾蓝     0xA25757
// 蒂芙尼蓝   0x7E272F
// 深青色     0xFF7F7F

// 樱花粉 0x00483A
// 浅粉色 0x003F34 0x00493E
// 亮粉色 0x00964B
// 珊瑚粉 0x077C86(偏橙)

// 中紫兰色 0x95A532(偏蓝)
// 电紫色   0x40FF00
// 芋泥紫   0x1F4F00