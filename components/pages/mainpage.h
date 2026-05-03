#ifndef __MAINPAGE_H__
#define __MAINPAGE_H__

#include "page_manager.h"

void main_page_remote_state_flush_func(const int *rocker, const uint16_t key, void *user_data);
void remote_flush_empty_func(const int *rocker, const uint16_t key, void *user_data);

#endif
