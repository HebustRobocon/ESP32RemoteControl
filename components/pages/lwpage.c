#include "lwpage.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern TaskHandle_t remote_state_task_handle;

void lw_page_create(void *user_data);
UI_PAGE_REGISTER("lw_page", lw_page_create);

static void lw_return_last_page_timer_cb(lv_timer_t *timer)
{
    lv_timer_del(timer);
    return_last_page();
}

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
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x121212), LV_PART_MAIN);
    
    // Header
    lv_obj_t *header_cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(header_cont, 320, 60);
    lv_obj_align(header_cont, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header_cont, lv_color_hex(0x1E1E1E), LV_PART_MAIN);
    lv_obj_set_style_border_width(header_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header_cont, 10, LV_PART_MAIN);
    
    lv_obj_t *title_label = lv_label_create(header_cont);
    lv_label_set_text(title_label, "LW PAGE");
    lv_obj_center(title_label);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x00E5FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, LV_PART_MAIN);
    
    // Info
    lv_obj_t *info_card = lv_obj_create(lv_screen_active());
    lv_obj_set_size(info_card, 290, 180);
    lv_obj_align(info_card, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_bg_color(info_card, lv_color_hex(0x1E1E1E), LV_PART_MAIN);
    lv_obj_set_style_radius(info_card, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(info_card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(info_card, 20, LV_PART_MAIN);
    
    lv_obj_t *info_label = lv_label_create(info_card);
    lv_label_set_text(info_label, "Welcome to the\nLW Page!\n\nThis is a custom\npage for your robot.");
    lv_obj_center(info_label);
    lv_label_set_long_mode(info_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(info_label, 250);
    lv_obj_set_style_text_color(info_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    
    // Back button
    lv_obj_t *action_button = lv_btn_create(lv_screen_active());
    lv_obj_set_size(action_button, 200, 50);
    lv_obj_align(action_button, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_color(action_button, lv_color_hex(0xFF5252), LV_PART_MAIN);
    lv_obj_set_style_bg_color(action_button, lv_color_hex(0xD32F2F), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(action_button, 25, LV_PART_MAIN);
    lv_obj_add_event_cb(action_button, lw_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(action_button);
    lv_label_set_text(btn_label, "BACK");
    lv_obj_center(btn_label);
    lv_obj_set_style_text_color(btn_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_16, LV_PART_MAIN);
}
