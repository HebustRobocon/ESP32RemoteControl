#ifndef __CORE_H__
#define __CORE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "dataFrame.h"

typedef void(*RemoteStateFlush_t)(const int *rockers, const uint16_t key,void* user_data);

void RemoteCoreInit();  //遥控器底层驱动组件初始化

//接口:
float get_battery_voltage();        //得到当前电池电压
void sys_shutdown();                //软件关闭遥控器
void sys_setup();                   //执行一次以维持遥控器开机状态
void set_remote_flush_func(RemoteStateFlush_t func,void* user_data); //修改刷新遥控器状态的函数
RemoteStateFlush_t get_remote_flush_func();                         //得到当前正在使用的遥控器状态刷新函数
void get_remote_state(float rocker_raw_data[4],uint16_t* key_data); //得到当前正在使用的队列


#endif