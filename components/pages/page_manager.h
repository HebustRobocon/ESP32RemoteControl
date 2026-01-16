#ifndef __PAGE_MANAGER_H__
#define __PAGE_MANAGER_H__

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "core.h"
#include "comm.h"
#include "data_poll.h"
#include "hardware.h"
#include "mylist.h"
#include "dataFrame.h"


typedef struct PagePort_t;

typedef void(*PageCreateCb)(void* user_data);

typedef struct{
    char *page_name;
    PageCreateCb create;
}PagePort_t;

// 声明由链接器生成的符号
extern uint8_t _ui_table_start;
extern uint8_t _ui_table_end;

#define UI_CONCAT_BASE(a, b) a##b
#define UI_CONCAT_EVAL(a, b) UI_CONCAT_BASE(a, b)



/**
 * @brief 注册新的LVGL页面
 * @param name_str 页面的名字（页面加载的依据）
 * @param create_cb 页面的创建函数
 */
#define UI_PAGE_REGISTER(name_str, create_cb) \
    const PagePort_t UI_CONCAT_EVAL(THIS_PAGE_ID, _page_port) \
    __attribute__((used, retain, section("ui_table"), aligned(4))) = { \
        .page_name = name_str, \
        .create = create_cb \
    }


inline uint32_t get_pages_num()
{
    const PagePort_t * start = (const PagePort_t *)&_ui_table_start;
    const PagePort_t * end   = (const PagePort_t *)&_ui_table_end;

    return (uint32_t)(end - start);
}


inline PagePort_t *get_pages_from_index(const uint32_t index)
{
    uint32_t num = get_pages_num();
    if (index >= num) {
        return NULL;
    }
    return (PagePort_t *)((&_ui_table_start) + index);
}

/**
 * @brief 页面管理器初始化函数
 * @param first_page_num 主页面名字
 * @param screen_mutex LVGL渲染互斥锁（mylvgl_port_init返回）
 * @return 成功加载页面返回1;找不到页面返回0
 */
uint32_t page_manager_init(const char *first_page_name,QueueHandle_t screen_mutex);

/**
 * @brief 获取LVGL渲染互斥锁
 * @return LVGL渲染互斥锁
 */
QueueHandle_t get_screen_mutex();

/**
 * @brief 跳转到某一个页面
 * @param next_page_name 新页面的名字
 * @param next_page_create_param 下一个页面创建时要传入的参数
 * @param this_page_create_param 返回到本页面，运行节点创建函数时要传入的参数
 */
uint32_t page_switch(const char* next_page_name,void *next_page_create_param,void* this_page_create_param);

uint32_t return_last_page();



#endif