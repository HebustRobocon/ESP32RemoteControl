#ifndef __MAINPAGE_H__
#define __MAINPAGE_H__

#include "page_manager.h"

typedef struct __attribute__((packed))
{
    uint8_t head; // 包头，比如可以定为 0xAA
    uint8_t data; // 数据位 (0 或 1)
    uint8_t tail; // 包尾，比如可以定为 0x55
} dock_pack_t;

void main_page_remote_state_flush_func(const int *rocker, const uint16_t key, void *user_data);
void remote_flush_empty_func(const int *rocker, const uint16_t key, void *user_data);

#endif
