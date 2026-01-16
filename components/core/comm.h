#ifndef __COMM_H__
#define __COMM_H__

#include <stdlib.h>

typedef void(*BadDataPackCb_t)(uint32_t type);

typedef void(*CommPackRecv_Cb)(uint8_t *src,uint16_t size,void* user_data);

typedef void(*CommPackSend_Cb)(void*user_data,uint32_t is_success);

/**
 * @brief 遥控器通信模块初始化
 * @param callback 接收到错误数据包时的回调
 * @return void
 */
void RemoteCommInit(BadDataPackCb_t callback);

/**
 * @brief 注册通信模块接收回调
 * @param callback 接收回调
 * @param cmd 命令字段（匹配时调用回调）
 * @param user_data 其它用户数据
 * @return 注册ID
 */
uint32_t register_comm_recv_cb(CommPackRecv_Cb callback,uint8_t cmd,void* user_data);

/**
 * @brief 取消注册通信模块接收回调
 * @param cb_id 注册ID
 * @return 0 失败；1 成功
 */
uint32_t unregister_comm_recv_cb(uint32_t cb_id);

/**
 * @brief  非阻塞方式发送一个数据包，放入发送队列后立即返回，不保证接收端正确接收
 * @param src 要发送的数据包内容
 * @param cmd 数据包命令字段
 * @param size 数据包的内容的长度（不包含命令字段）
 */
uint32_t asyn_comm_send_pack_nak(uint8_t *src,uint8_t cmd,uint16_t size);

/**
 * @brief 阻塞方式发送一个数据包，并且等待接收端应答直到超时或者
 * @param src 要发送的数据包内容
 * @param cmd 数据包命令字段
 * @param size 数据包的内容的长度（不包含命令字段）
 * @param time_out_ms 每次发送等待ACK包的超时时间（ms）
 * @param max_retry_num 最大的尝试重发次数
 * @return 1发送成功；0发送失败（没有收到应道/发送请求队列满/ACK挂起线程池满）
 */
uint32_t comm_send_pack_ack(uint8_t *src, uint8_t cmd, uint16_t size, uint32_t time_out_ms, uint8_t max_retry_num);

/**
 *  * @brief 阻塞方式发送一个数据包，并且等待接收端应答直到超时或者
 * @param src 要发送的数据包内容
 * @param cmd 数据包命令字段
 * @param size 数据包的内容的长度（不包含命令字段）
 * @param send_cb 发送完成回调（超时或者收到应答包）
 * @param user_data 回调函数其它用户数据
 * @param time_out_ms 每次发送等待ACK包的超时时间（ms）
 * @param max_retry_num 最大的尝试重发次数
 * @return 1发送成功；0发送失败（没有收到应道/发送请求队列满/ACK挂起线程池满）
 */
uint32_t asyn_comm_send_pack_ack(uint8_t *src,uint8_t cmd,uint16_t size,CommPackSend_Cb send_cb,void* user_data,uint8_t max_retry_num);

#endif