# 河北科技大学Robocon机器人遥控器

## 简介

### 硬件：

- 主控 ESP32-S3

- ILI9341显示芯片/FT6636电容屏触摸芯片-240*360分辨率

- 74LS165移位寄存器扫描按键和拨杆

- 使用基于串口的LORA模块实现无线通信

- 2自由度模拟信号摇杆

- 1-bit模式-SD卡槽

  ### 软件：

- 基于官方ESP-IDF-v5.3.2框架
- 使用FatFs实现SD卡文件系统
- 使用LVGL9实现UI页面

## 目录结构

```
├── components
├────core			遥控器核心功能（通信/按键与摇杆扫描/电源管理）
├────fatfs			FatFs文件系统
├────lvgl			LVGL框架
├────pages			负责实现所有UI界面创建，销毁，事件处理
├────screen			屏幕驱动接口
├── main
│   ├── CMakeLists.txt
│   └── main.c		程序入口
├── README.md
└─*.*
```

## 启动流程

1. ESP32-S3 bootloader启动，初始化系统环境，进入app_main()
2. 调用一次sys_setup()维持系统上电状态
3. 初始化屏幕和触摸芯片
4. LVGL初始化，注册底层驱动
5. 调用page_manager_init初始化页面管理器，并初始化加载"main_page"页面
6. main_page页面初始化函数会初始化串口通信功能，创建硬件扫描任务，并创建主页面的UI元素

## 二次开发

### 接口

1. core-comm.c/.h提供了用于与机器人通信的，保证数据可靠的接口
2. core-core.c/.h提供了访问遥控器底层硬件的接口
3. core-data_poll.c/.h提供了静态内存池方法
4. core-mylist.c/.h提供了基本的单向链表功能（线程不安全）
5. core-hardware.h提供了ESP32所连接的外设的引脚/外设宏定义
6. core-dataFrame.h声明了与机器人通信时的一些数据包结构
7. pages-page_manager.c/.h提供了页面之间跳转，查询，页面注册等接口

### 创建页面

1. 编写一个页面的创建函数用于初始化当前页面的资源，在以下情形下，该函数会被调用：

   - 启动时被页面管理器加载作为主页面

   - 其他页面通过调用page_switch首次跳转到当前页面

   - 其他页面通过调用return_last_page返回到本页面

     因此，在多个页面共存的情况下，需要代码编写者注意内存泄漏和内存提前销毁方面的问题

2. 使用page_manager.h提供的宏函数UI_PAGE_REGISTER()注册你的页面，一个.c文件只能调用一次UI_PAGE_REGISTER()函数，也就是说，一个.c文件可以负责最多一个UI界面的创建

3. **在任何情况下，绝对不可以在页面创建函数（以下称做page_create函数）中调用以下函数xSemaphoreTake(get_screen_mutex(),portMAX_DELAY);/xSemaphoreGive(get_screen_mutex());考虑实际情况，page_switch/return_last_page只是在各类按钮/LVGL定时器的回调中执行。这类回调已经被互斥锁保护了，所以不能在这种情况下再次尝试获取锁。**

4. **当页面任务比较复杂，只依赖定时器和各类控件的回调难以实现需求时，可以创建并维护一个FreeRTOS线程。如果需要在线程中更新UI，则必须使用get_screen_mutex();获取负责保护LVGL渲染的互斥锁并调用xSemaphoreTake/xSemaphoreGive确保在UI更新和渲染不会同时发生。**
