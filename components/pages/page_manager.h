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

// 第二层：中间层，用来强制展开参数 a (即 THIS_PAGE_ID)
#define UI_CONCAT_EVAL(a, b) UI_CONCAT_BASE(a, b)

// 第三层：用户调用的宏
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


uint32_t page_manager_init(const char *first_page_name,QueueHandle_t screen_mutex);

QueueHandle_t get_screen_mutex();

uint32_t page_switch(const char* next_page_name,void *next_page_create_param,void* this_page_create_param);

uint32_t return_last_page();



#endif