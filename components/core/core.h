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

/**
 * @brief 获取电池电压
 * @return 电池电压(V)
 */
float get_battery_voltage();

/**
 * @brief 关闭遥控器
 */
void sys_shutdown();

/**
 * @brief 上电时执行一次以维持遥控器开机状态
 */
void sys_setup();

/**
 * @brief 设置遥控器状态更新函数。按键/摇杆状态在不断更新，因此有一个FreeRTOS任务会不断扫描他们的状态。
 * 该函数会在扫描完成后被调用，频率大约为50Hz，该函数不可以有任何阻塞行为
 * @param func 新的遥控器状态更新函数
 * @param user_data 其它用户数据
 * @note 最好不要在遥控器状态更新函数操作UI，因为在这里操作UI必须等待LVGL渲染的互斥锁释放，可能造成控制指令的延迟
 * @return void
 */
void set_remote_flush_func(RemoteStateFlush_t func,void* user_data);

/**
 * @brief 得到当前的遥控器状态更新函数，通常用来实现在保留原有功能的条件下，实现附加的新功能。
 * @return 遥控器状态更新函数
 */
RemoteStateFlush_t get_remote_flush_func();                         //得到当前正在使用的遥控器状态刷新函数

/**
 * @brief 得到当前遥控器状态，该方法侵入性更小，使用更方便，但是只能读取，不能修改
 * @param rocker_raw_data 摇杆ADC原始数据（理想条件下0~4095）
 * @param key_data 按键数据
 */
void get_remote_state(int rocker_raw_data[4],uint16_t* key_data);


#endif