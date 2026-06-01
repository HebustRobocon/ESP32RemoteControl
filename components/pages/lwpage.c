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
    STATE_NONE = 0,  // 绿色 - 无
    STATE_R1 = 1,    // 黄色 - R1-KFS
    STATE_R2 = 2,    // 红色 - R2-KFS
    STATE_False = 3, // 粉色 - F

    STATE_COUNT // 注:利用枚举自动递增的特性，STATE_COUNT 的数值正好等于前面所有有效状态的总数
} block_state_t;
static block_state_t block_states[12] = {STATE_NONE}; // 存储 12 个块的状态
static uint8_t Send_Pack[12] = {0};                   // 用于发送的数组，存储 12 个块的状态
// 延迟返回上一页面，避免在事件回调中直接删除当前屏幕
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

    // 状态循环切换(N -> F -> R1 -> R2 -> N)
    block_states[index] = (block_states[index] + 1) % STATE_COUNT;

    // 根据状态更新 UI
    switch (block_states[index])
    {
    case STATE_NONE:
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF007F), 0); // 暗绿色
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
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x00964B), 0); // 粉色 (Pink~)
        lv_label_set_text(label, "F");
        break;
    default:
        break;
    }
}
// "Send"按钮的回调函数
// "Send"按钮的回调函数
static void send_grid_data_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        int r1_in_set_count = 0; // 指定集合中 R1 的数量
        int total_r2_count = 0;  // 整个方格中 R2 的数量
        int total_f_count = 0;   // 整个方格中 F 的数量

        // 遍历所有 12 个方格进行统计
        for (int i = 0; i < 12; i++)
        {
            // 1. 统计整个方格的 R2 和 F 数量
            if (block_states[i] == STATE_R2)
            {
                total_r2_count++;
            }
            else if (block_states[i] == STATE_False) // STATE_False 对应文字 "F"
            {
                total_f_count++;
            }

            // 2. 统计指定按键集合（0,3,6,9,10,11,8,5,2,1）中 R1 的数量
            if (i != 4 && i != 7)
            {
                if (block_states[i] == STATE_R1)
                {
                    r1_in_set_count++;
                }
            }
        }

        // 判断是否满足所有条件：
        // 条件1：指定集合中[存在且只有] 3 个 R1
        // 条件2：整个方格中[存在且只有] 4 个 R2
        // 条件3：整个方格中[存在且只有] 1 个 F
        if (r1_in_set_count == 3 && total_r2_count == 4 && total_f_count == 1)
        {
            // 满足条件，允许执行发送
            for (int i = 0; i < 12; i++)
            {
                Send_Pack[i] = (uint8_t)block_states[i];
            }
            asyn_comm_send_pack_nak(Send_Pack, PACK_LW_GRID_CMD, sizeof(Send_Pack));
            printf("Data sent successfully. Data size: %d\n", (int)sizeof(Send_Pack));
        }
        else
        {
            // 不满足条件，拒绝发送（你可以在这里添加弹窗提示或蜂鸣器提示）
            printf("Send blocked! Current: R1(in set)=%d/3, R2(total)=%d/4, F(total)=%d/1\n",
                   r1_in_set_count, total_r2_count, total_f_count);
        }
    }
}
// static void send_grid_data_event_cb(lv_event_t *e)
// {
//     lv_event_code_t code = lv_event_get_code(e);
//     if (code == LV_EVENT_CLICKED)
//     {
//         // 调用发送函数，发送 12 个方格的状态
//         // 数据长度为 12 * sizeof(block_state_t)
//         for (int i = 0; i < 12; i++)
//         {
//             Send_Pack[i] = (uint8_t)block_states[i];
//         }
//         asyn_comm_send_pack_nak(Send_Pack, PACK_LW_GRID_CMD, sizeof(Send_Pack)); // sizeof(Send_Pack)
//         printf("Data size: %d\n", sizeof(Send_Pack));
//     }
// }
void lw_page_create(void *user_data)
{
    // 3*4 UI页面
    lv_obj_t *cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(cont, 240, 250);            // 适配你的屏幕，留点空间给底部的按钮
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 0); // 靠顶对齐

    // 设置 Flex 布局 (网格样式)
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont, 5, 0);
    lv_obj_set_style_pad_gap(cont, 5, 0);

    // 循环创建 12 个方块 (3 * 4)
    for (int i = 0; i < 12; i++)
    {
        // 计算映射后的索引：当 i=0(左上角) 时，index=11；当 i=11(右下角) 时，index=0
        int real_index = 11 - i;

        lv_obj_t *btn = lv_button_create(cont);
        lv_obj_set_size(btn, 55, 55);
        // 设置初始样式 (紫色)
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF007F), 0);

        // 创建文字
        lv_obj_t *label = lv_label_create(btn);

        // 状态恢复逻辑
        switch (block_states[real_index])
        {
        case STATE_NONE:
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF007F), 0); // 紫色0x00FF00
            lv_label_set_text(label, "N");
            break;
        case STATE_R1:
            lv_obj_set_style_bg_color(btn, lv_color_hex(0X00FF00), 0); //紫色
            lv_label_set_text(label, "R1");
            break;
        case STATE_R2:
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF0000), 0);//浅蓝
            lv_label_set_text(label, "R2");
            break;
        case STATE_False:
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x00964B), 0); // 粉色
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
    lv_obj_set_style_pad_gap(bottom_cont, 20, 0);
    lv_obj_set_style_border_side(bottom_cont, LV_BORDER_SIDE_NONE, 0); // 隐藏边框

    // 1. 创建“发送数据”按钮
    lv_obj_t *send_btn = lv_button_create(bottom_cont);
    lv_obj_set_size(send_btn, 80, 45);
    lv_obj_add_event_cb(send_btn, send_grid_data_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(send_btn, lv_palette_main(LV_PALETTE_BLUE), 0);

    lv_obj_t *send_label = lv_label_create(send_btn);
    lv_label_set_text(send_label, "SEND");
    lv_obj_center(send_label);

    // 2. 创建“返回”按钮 (把你原来的返回逻辑移到这里)
    lv_obj_t *back_btn = lv_button_create(bottom_cont);
    lv_obj_set_size(back_btn, 80, 45);
    lv_obj_set_style_bg_color(back_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(back_btn, lw_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "BACK");
    lv_obj_center(back_label);
}