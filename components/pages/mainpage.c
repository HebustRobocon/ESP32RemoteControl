#include "mainpage.h"
#include "lvgl/lvgl.h"
#include "math.h"
#include "lvgl/lv_conf.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "core.h"
#include "comm.h"

void main_page_create(void *user_data);
void main_page_remote_state_flush_func(const int *rocker, const uint16_t key, void *user_data);
UI_PAGE_REGISTER("main_page", main_page_create);
static uint8_t main_page_created_flag = 0;

static uint8_t dock_state = 0;   // 初始化为0的对接变量
static lv_obj_t *dock_btn;       // 对接按钮句柄
static lv_obj_t *dock_btn_label; // 对接按钮文字句柄

void remote_flush_empty_func(const int *rocker, const uint16_t key, void *user_data)
{
    // 什么都不做，也不释放信号量，彻底切断发送触发链
}

static lv_obj_t *rocker_label;
lv_obj_t *keys_state_label;
static int remote_rocker[4] = {0};
static uint16_t remote_key = 0;
static SemaphoreHandle_t remote_data_semaphore = NULL;
static volatile uint8_t labels_created = 0;

// 全局任务句柄，供其他文件访问
TaskHandle_t remote_state_task_handle = NULL;

static void remote_state_task(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t update_interval = pdMS_TO_TICKS(20); // 20ms更新间隔

    while (1)
    {
        // 等待新的远程数据或超时
        if (remote_data_semaphore != NULL)
        {
            xSemaphoreTake(remote_data_semaphore, update_interval);
        }
        else
        {
            // 信号量未创建，直接延迟
            vTaskDelay(update_interval);
            continue;
        }

        // 检查屏幕是否存在
        lv_obj_t *scr = lv_screen_active();
        if (scr == NULL)
        {
            // 没有屏幕，跳过UI更新
            vTaskDelay(update_interval);
            continue;
        }

        // 检查标签是否仍然有效（移除了 battery_label 的校验）
        if (!lv_obj_is_valid(keys_state_label) || !lv_obj_is_valid(rocker_label))
        {
            // 标签无效，跳过UI更新
            vTaskDelay(update_interval);
            continue;
        }

        // 无论是否有新数据，都按固定频率处理
        xSemaphoreTake(get_screen_mutex(), portMAX_DELAY);
        // 再次检查标签是否仍然有效（移除了 battery_label 的校验）
        if (lv_obj_is_valid(keys_state_label) && lv_obj_is_valid(rocker_label))
        {
            char out_str[8] = {0};
            sprintf(out_str, "0x%X", remote_key);
            lv_label_set_text(keys_state_label, out_str);

            // 直接使用处理后的值，不需要再计算
            int *rocker_processed = (int *)remote_rocker;

            char rocker_str[64] = {0};
            sprintf(rocker_str, "Rocker: %d,%d,%d,%d", rocker_processed[0], rocker_processed[1], rocker_processed[2], rocker_processed[3]);
            lv_label_set_text(rocker_label, rocker_str);
        }
        xSemaphoreGive(get_screen_mutex());

        // 发送数据，无论UI是否更新成功
        static PackControl_t remoteInfo;
        remoteInfo.Key = remote_key;
        static float rocker_last[4] = {0};
        for (int i = 0; i < 4; i++)
        {
            remote_rocker[i] = 0.17f * remote_rocker[i] + (1.0f - 0.17f) * rocker_last[i];
            rocker_last[i] = remote_rocker[i]; // 更新历史值
            remoteInfo.rocker[i] = remote_rocker[i];
        }
        asyn_comm_send_pack_nak((uint8_t *)&remoteInfo, 0x01, sizeof(remoteInfo));

        // 使用vTaskDelayUntil确保固定频率
        vTaskDelayUntil(&last_wake_time, update_interval);
    }
}

// 按钮事件回调 - 跳转到lwpage
// 对接按钮点击事件回调
static void lwpage_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        set_remote_flush_func(remote_flush_empty_func, NULL); // 停止底层硬件对回调的触发
        printf("Button clicked, switching to lw_page\r\n");
        if (remote_state_task_handle != NULL)
        {
            vTaskSuspend(remote_state_task_handle);
            printf("Remote state task suspended\r\n");
        }
        uint32_t result = page_switch("lw_page", NULL, NULL);
        printf("page_switch result: %lu\r\n", result);
    }
}

static void obj_page_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        set_remote_flush_func(remote_flush_empty_func, NULL); // 停止底层硬件对回调的触发
        printf("Button clicked, switching to obj_page\r\n");
        if (remote_state_task_handle != NULL)
        {
            vTaskSuspend(remote_state_task_handle);
            printf("Remote state task suspended\r\n");
        }
        uint32_t result = page_switch("obj_page", NULL, NULL);
        printf("page_switch result: %lu\r\n", result);
    }
}

void main_page_remote_state_flush_func(const int *rocker, const uint16_t key, void *user_data)
{
    memcpy(remote_rocker, rocker, sizeof(remote_rocker));
    remote_key = key;

    if (remote_data_semaphore != NULL)
    {
        xSemaphoreGive(remote_data_semaphore);
    }
}
static void dock_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        // 1. 状态翻转 (0->1, 1->0)
        dock_state = !dock_state;

        // 2. 动态更新 UI 样式
        if (dock_state == 1)
        {
            // 变绿，显示 "T"
            lv_obj_set_style_bg_color(dock_btn, lv_color_hex(0xFF007F), 0);
            lv_label_set_text(dock_btn_label, "T");
        }
        else
        {
            // 变红，显示 "F"
            lv_obj_set_style_bg_color(dock_btn, lv_color_hex(0x00964B), 0);
            lv_label_set_text(dock_btn_label, "F");
        }

        // 3. 组装结构体数据包
        dock_pack_t pack;
        pack.head = 0xAA;       // 自定义包头
        pack.data = dock_state; // 核心数据 (0 或 1)
        pack.tail = 0xBB;       // 自定义包尾

        asyn_comm_send_pack_nak((uint8_t *)&pack, 0x06, sizeof(pack));
        printf("Dock button pressed. State: %d, Pack size: %d\n", dock_state, (int)sizeof(pack));
    }
}

void main_page_create(void *user_data)
{
    printf("[MAIN_PAGE] ===== Page creation started =====\r\n");

    if (main_page_created_flag)
    {
        printf("[MAIN_PAGE] Page already created, skipping\r\n");
        return;
    }
    main_page_created_flag = 1;
    // 创建信号量用于远程数据同步
    remote_data_semaphore = xSemaphoreCreateBinary();
    if (remote_data_semaphore == NULL)
    {
        printf("[MAIN_PAGE] Failed to create remote data semaphore\n");
    }

    // 创建远程状态任务，优先级低于CoreTask
    BaseType_t task_create_result = xTaskCreate(remote_state_task, "remote_state_task", 4096, NULL, 4, &remote_state_task_handle);
    if (task_create_result != pdPASS)
    {
        printf("[MAIN_PAGE] Failed to create remote state task\n");
    }
    // 通信模块初始化
    RemoteCommInit(NULL);
    // 硬件状态更新任务初始化
    RemoteCoreInit();

    keys_state_label = lv_label_create(lv_screen_active());
    lv_label_set_text(keys_state_label, "key:");
    lv_obj_align(keys_state_label, LV_ALIGN_TOP_MID, 0, 0);
    set_remote_flush_func(main_page_remote_state_flush_func, keys_state_label);

    rocker_label = lv_label_create(lv_screen_active());
    lv_label_set_text(rocker_label, "Rocker: 0,0,0,0");
    lv_obj_align(rocker_label, LV_ALIGN_TOP_MID, 0, 50);

    // 所有安全标签创建完成
    labels_created = 1;
    /**标签创建**/

    // 1. "L"标签
    lv_obj_t *L_label = lv_label_create(lv_screen_active());
    lv_label_set_text(L_label, "L");
    // 创建样式
    static lv_style_t style_L;
    lv_style_init(&style_L);
    lv_style_set_text_color(&style_L, lv_color_hex(0xFF0000));//蓝色
    lv_style_set_text_font(&style_L, &lv_font_montserrat_48);
    // 应用样式
    lv_obj_add_style(L_label, &style_L, 0);
    lv_obj_align(L_label, LV_ALIGN_TOP_MID, -80, 200);

    // 2. "M"标签
    lv_obj_t *M_label = lv_label_create(lv_screen_active());
    // 创建样式
    lv_label_set_text(M_label, "M");
    static lv_style_t style_M;
    lv_style_init(&style_M);
    lv_style_set_text_color(&style_M, lv_color_hex(0x003F34));//粉色
    lv_style_set_text_font(&style_M, &lv_font_montserrat_48);
    // 应用样式
    lv_obj_add_style(M_label, &style_M, 0);
    lv_obj_align(M_label, LV_ALIGN_TOP_MID, 80, 200);

    // 3. "第一"标签
    lv_obj_t *z_label = lv_label_create(lv_screen_active());
    // 使用UTF-8编码的字符串，避免中文乱码
    lv_label_set_text(z_label, " 第 一 !!!");
    // 创建样式
    static lv_style_t style_Z;
    lv_style_init(&style_Z);
    lv_style_set_text_color(&style_Z, lv_color_hex(0xFF00FF));
    // 使用支持中文的SimSun字体
    lv_style_set_text_font(&style_Z, &lv_font_simsun_16_cjk);
    // 应用样式
    lv_obj_add_style(z_label, &style_Z, 0);
    lv_obj_align(z_label, LV_ALIGN_TOP_MID, 0, 300);

    /* --- 按钮布局 --- */
    // 1. 创建左侧的 "LW Page" 按钮
    lv_obj_t *lwpage_button = lv_btn_create(lv_screen_active());
    lv_obj_set_size(lwpage_button, 100, 50);
    // 对齐到左下角，水平偏移 15，垂直偏移 -20
    lv_obj_align(lwpage_button, LV_ALIGN_BOTTOM_LEFT, 15, -20);
    lv_obj_add_event_cb(lwpage_button, lwpage_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lwpage_btn_label = lv_label_create(lwpage_button);
    lv_label_set_text(lwpage_btn_label, "LW Page");
    lv_obj_center(lwpage_btn_label);
    // 2. 创建右侧的 "OBJ Page" 按钮
    lv_obj_t *obj_page_button = lv_btn_create(lv_screen_active());
    lv_obj_set_size(obj_page_button, 100, 50);
    // 对齐到右下角，水平偏移 -15，垂直偏移 -20
    lv_obj_align(obj_page_button, LV_ALIGN_BOTTOM_RIGHT, -15, -20);
    lv_obj_add_event_cb(obj_page_button, obj_page_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *obj_btn_label = lv_label_create(obj_page_button);
    lv_label_set_text(obj_btn_label, "obj_Page");
    lv_obj_center(obj_btn_label);

    //3.创建中心对接按钮
    dock_btn = lv_btn_create(lv_screen_active());
    lv_obj_set_size(dock_btn, 120, 120);                              // 宽高60的精美圆形
    lv_obj_set_style_radius(dock_btn, LV_RADIUS_CIRCLE, 0);         // 圆形
    lv_obj_set_style_bg_color(dock_btn, lv_color_hex(0x00964B), 0); // 初始为红色
    // 居中靠顶对齐，向下偏移 110px 避开Rocker标签，正好贴入正中空白区
    lv_obj_align(dock_btn, LV_ALIGN_TOP_MID, 0, 90);
    lv_obj_add_event_cb(dock_btn, dock_btn_event_cb, LV_EVENT_CLICKED, NULL);

    dock_btn_label = lv_label_create(dock_btn);
    lv_obj_set_style_text_font(dock_btn_label, &lv_font_montserrat_48, 0);
    lv_label_set_text(dock_btn_label, "F"); // 初始显示 F
    lv_obj_center(dock_btn_label);

    /* --- 按钮布局结束 --- */
    printf("[MAIN_PAGE] ===== Page creation completed =====\r\n");

    if (remote_state_task_handle != NULL)
    {
        vTaskResume(remote_state_task_handle);
        printf("[MAIN_PAGE] Remote state task resumed\r\n");
    }
}

