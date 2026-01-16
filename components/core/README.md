# 本组件负责遥控器核心功能

- core.c/core.h  启动按键/摇杆扫描任务，初始化上下行通信链路，初始化电源管理部分
- comm.c/comm.h 以串口（LORO模块）为实际链路实现的上下行通信链路
- dataFrame.h 上下行通信数据包格式
- hardware.h 硬件接口
