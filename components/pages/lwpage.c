#include "lwpage.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mainpage.h"

#define PACK_LW_GRID_CMD 0x05

extern lv_obj_t *keys_state_label;
extern TaskHandle_t remote_state_task_handle;
void lw_page_create(void *user_data);
UI_PAGE_REGISTER("lw_page", lw_page_create);

typedef enum
{
    STATE_NONE = 0,  // 绿色 - 无（注：你注释写绿色/紫色，代码实际对应0xFF007F）
    STATE_R1 = 1,    // 黄色 - R1-KFS
    STATE_R2 = 2,    // 红色 - R2-KFS
    STATE_False = 3, // 粉色 - F

    STATE_COUNT // 利用枚举自动递增的特性
} block_state_t;

static block_state_t block_states[12] = {STATE_NONE}; // 存储 12 个块的状态
static uint8_t Send_Pack[12] = {0};                   // 用于发送的数组

static lv_obj_t *grid_container = NULL; // 提取成静态变量，方便清空回调获取子对象

// 延迟返回上一页面
static void lw_return_last_page_timer_cb(lv_timer_t *timer)
{
    lv_timer_del(timer);
    return_last_page();
}

// "Back"按钮事件回调
static void lw_btn_event_cb(lv_event_t *e)
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

// 3*4网格按钮回调
static void grid_btn_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);           // 获取按钮里的 label
    intptr_t index = (intptr_t)lv_event_get_user_data(e); // 获取是哪个按钮

    // 状态循环切换(N -> R1 -> R2 -> F -> N)
    block_states[index] = (block_states[index] + 1) % STATE_COUNT;

    // 根据状态更新 UI
    switch (block_states[index])
    {
    case STATE_NONE:
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF007F), 0); // 初始色
        lv_label_set_text(label, "N");
        break;
    case STATE_R1:
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x40FF00), 0);
        lv_label_set_text(label, "R1");
        break;
    case STATE_R2:
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF0000), 0);
        lv_label_set_text(label, "R2");
        break;
    case STATE_False:
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x00964B), 0);
        lv_label_set_text(label, "F");
        break;
    default:
        break;
    }
}

// "Send"按钮的回调函数
static void send_grid_data_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        int r1_in_set_count = 0; // 指定集合中 R1 的数量
        int total_r2_count = 0;  // 整个方格中 R2 的数量
        int total_f_count = 0;   // 整个方格中 F 的数量

        for (int i = 0; i < 12; i++)
        {
            if (block_states[i] == STATE_R2)
                total_r2_count++;
            else if (block_states[i] == STATE_False)
                total_f_count++;

            if (i != 4 && i != 7)
            {
                if (block_states[i] == STATE_R1)
                    r1_in_set_count++;
            }
        }

        if (r1_in_set_count == 3 && total_r2_count == 4 && total_f_count == 1)
        {
            for (int i = 0; i < 12; i++)
            {
                Send_Pack[i] = (uint8_t)block_states[i];
            }
            asyn_comm_send_pack_nak(Send_Pack, PACK_LW_GRID_CMD, sizeof(Send_Pack));
            printf("Data sent successfully. Data size: %d\n", (int)sizeof(Send_Pack));
        }
        else
        {
            printf("Send blocked! Current: R1(in set)=%d/3, R2(total)=%d/4, F(total)=%d/1\n",
                   r1_in_set_count, total_r2_count, total_f_count);
        }
    }
}

// ✨ 新增：“清空”按钮的回调函数
static void clear_grid_data_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        // 1. 清空数据状态变量
        for (int i = 0; i < 12; i++)
        {
            block_states[i] = STATE_NONE;
        }

        // 2. 遍历网格容器中的所有按钮子对象，刷新 UI 状态
        if (grid_container != NULL)
        {
            uint32_t child_count = lv_obj_get_child_count(grid_container);
            for (uint32_t i = 0; i < child_count; i++)
            {
                lv_obj_t *btn = lv_obj_get_child(grid_container, i);
                if (btn != NULL)
                {
                    // 恢复初始颜色
                    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF007F), 0);
                    // 恢复初始文字 "N"
                    lv_obj_t *label = lv_obj_get_child(btn, 0);
                    if (label != NULL)
                    {
                        lv_label_set_text(label, "N");
                    }
                }
            }
        }
        //3. 发送清空数据
        asyn_comm_send_pack_nak(block_states, PACK_LW_GRID_CMD, sizeof(block_states));
        printf("Grid cleared successfully.\n");
    }
}

void lw_page_create(void *user_data)
{
    // 3*4 UI页面容器
    grid_container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(grid_container, 240, 250);            // 适配屏幕
    lv_obj_align(grid_container, LV_ALIGN_TOP_MID, 0, 0); // 靠顶对齐

    // 设置 Flex 布局 (网格样式)
    lv_obj_set_flex_flow(grid_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(grid_container, 5, 0);
    lv_obj_set_style_pad_gap(grid_container, 5, 0);

    // 循环创建 12 个方块 (3 * 4)
    for (int i = 0; i < 12; i++)
    {
        int real_index = 11 - i;

        lv_obj_t *btn = lv_button_create(grid_container);
        lv_obj_set_size(btn, 55, 55);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF007F), 0);

        lv_obj_t *label = lv_label_create(btn);

        // 状态恢复逻辑
        switch (block_states[real_index])
        {
        case STATE_NONE:
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF007F), 0);
            lv_label_set_text(label, "N");
            break;
        case STATE_R1:
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x00FF00), 0);
            lv_label_set_text(label, "R1");
            break;
        case STATE_R2:
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF0000), 0);
            lv_label_set_text(label, "R2");
            break;
        case STATE_False:
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x00964B), 0);
            lv_label_set_text(label, "F");
            break;
        default:
            break;
        }
        lv_obj_center(label);
        lv_obj_add_event_cb(btn, grid_btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)real_index);
    }

    // 创建底部控制栏容器
    lv_obj_t *bottom_cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(bottom_cont, 240, 80);
    lv_obj_align(bottom_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(bottom_cont, LV_FLEX_FLOW_ROW); // 横向排列
    lv_obj_set_flex_align(bottom_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(bottom_cont, 10, 0);                      // 稍微缩小间距以容纳 3 个按钮
    lv_obj_set_style_border_side(bottom_cont, LV_BORDER_SIDE_NONE, 0); // 隐藏边框

    // 1. 创建“发送数据”按钮
    lv_obj_t *send_btn = lv_button_create(bottom_cont);
    lv_obj_set_size(send_btn, 65, 45); // 调整了宽度
    lv_obj_add_event_cb(send_btn, send_grid_data_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(send_btn, lv_palette_main(LV_PALETTE_BLUE), 0);

    lv_obj_t *send_label = lv_label_create(send_btn);
    lv_label_set_text(send_label, "SEND");
    lv_obj_center(send_label);

    // ✨ 2. 新增：居中的“清空”圆形按钮
    lv_obj_t *clear_btn = lv_button_create(bottom_cont);
    lv_obj_set_size(clear_btn, 45, 45);                      // 宽高相等以确保是正圆
    lv_obj_set_style_radius(clear_btn, LV_RADIUS_CIRCLE, 0); // 设置圆角为正圆
    lv_obj_add_event_cb(clear_btn, clear_grid_data_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(clear_btn, lv_color_hex(0x783114), 0); // 橘色醒目提示

    lv_obj_t *clear_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_label, "CLR"); // 缩写以适配圆形大小
    lv_obj_center(clear_label);

    // 3. 创建“返回”按钮
    lv_obj_t *back_btn = lv_button_create(bottom_cont);
    lv_obj_set_size(back_btn, 65, 45); // 调整了宽度
    lv_obj_set_style_bg_color(back_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(back_btn, lw_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "BACK");
    lv_obj_center(back_label);
}