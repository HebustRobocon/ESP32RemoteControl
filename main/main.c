#include <stdint.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "ILI9341.h"
#include "FT6336.h"
#include "comm.h"
#include "core.h"
#include "lvgl_port_disp.h"
#include "page_manager.h"

#include "lvgl/lvgl.h"
#include "lvgl.h"



#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

void SDInit();
void FatFsInit();

void app_main(void)
{
    sys_setup();
    //TODO:NVS初始化

    //屏幕显示初始化
    ILI9341_Init();
    FT6636_init();
    lv_init();
    QueueHandle_t screen_mutex=mylvgl_port_init();
    page_manager_init("main_page",screen_mutex);
    
    SDInit();
    while (1)
    {
        printf("running...\r\n");
        vTaskDelay(5000);
    }
}

void SDInit()
{
    esp_err_t ret;
    sdmmc_card_t *card;

    // 1. SDMMC Host 配置
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    // 2. Slot 配置（ESP32-S3 使用 slot 1）
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // 使用 1-bit 或 4-bit
    slot_config.width = 1;
    slot_config.clk = GPIO_NUM_8;
    slot_config.cmd = GPIO_NUM_16;
    slot_config.d0  = GPIO_NUM_1;

    // 3. 挂载参数
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, // 是否自动格式化
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // 4. 挂载
    ret = esp_vfs_fat_sdmmc_mount(
        "/sdcard",     // 挂载点
        &host,
        &slot_config,
        &mount_config,
        &card
    );

    if (ret != ESP_OK) {
        printf("Failed to mount SD card (%s)", esp_err_to_name(ret));
        return ;
    }

    // 5. 打印 SD 卡信息
    sdmmc_card_print_info(stdout, card);

    printf("SD card mounted at /sdcard");
}

void FatFsInit()
{

}
