#ifndef __DATAFRAME_H__
#define __DATAFRAME_H__

#include <stdint.h>

#define PACK_HEAD 0x5A
#define ACK_HEAD 0xAA

#define PACK_TYPE_MASK  0x80
#define PACK_CMD_MASK   0x0F

#define PACK_TYPE_ACK   0x80
#define PACK_TYPE_NAK   0x00

#define CMD_REMOTE_UPDATE_ROCKER            0x01
#define CMD_REMOTE_UPDATE_VIRTUAL_ITEM      0x02

#define CMD_RECEIVER_MESSAGEBOX             0x03
#define CMD_RECEIVER_UPDATE_VIRTUAL_ITEM    0x04

#define Right_Switch_Up     0x02
#define Right_Switch_Down   0x04
#define Right_Key_Up        0x08
#define Right_Key_Left      0x10
#define Right_Key_Right     0x20
#define Right_Key_Down      0x40
#define Left_Broadside_Key  0x80
#define Right_Broadside_Key 0x100
#define Left_Switch_Down    0x200
#define Left_Switch_Up      0x400
#define Left_Key_Down       0x800
#define Left_Key_Left       0x1000
#define Left_Key_Right      0x2000
#define Left_Key_Up         0x4000

#pragma pack(1)

//遥控器下行数据包，控制信号
#define PACK_CONTROL_CMD    0x01
typedef struct
{
	float rocker[4];
	uint32_t Key;
}PackControl_t;

//遥控器上行数据包，字符串反馈信息
#define PACK_STR_FEEDBACK_CMD    0x02
typedef struct
{
    uint8_t type;
    uint8_t size;
    char* str;
}PackMsg_t;
#pragma pack()



#endif