#ifndef __COMM_H__
#define __COMM_H__

#include <stdlib.h>

typedef void(*BadDataPackCb_t)(uint32_t type);

typedef void(*CommPackRecv_Cb)(uint8_t *src,uint16_t size,void* user_data);
typedef void(*CommPackSend_Cb)(void*user_data,uint32_t is_success);

void RemoteCommInit(BadDataPackCb_t callback);

//通信操作接口
void set_recv_error_cb(BadDataPackCb_t callback);
uint32_t register_comm_recv_cb(CommPackRecv_Cb callback,uint8_t cmd,void* user_data);       //注册上行数据包接收回调
uint32_t unregister_comm_recv_cb(uint32_t cb_id);                               //取消注册数据包(不建议频繁使用)
uint32_t asyn_comm_send_pack_nak(uint8_t *src,uint8_t cmd,uint16_t size);                                                  //下行数据包发送
uint32_t comm_send_pack_ack(uint8_t *src,uint8_t cmd,uint16_t size,CommPackSend_Cb send_cb,void* user_data,uint32_t time_out_ms,uint8_t max_retry_num);                                                  //下行数据包发送(待ACK确认)
uint32_t asyn_comm_send_pack_ack(uint8_t *src,uint8_t cmd,uint16_t size,CommPackSend_Cb send_cb,void* user_data,uint8_t max_retry_num);

#endif