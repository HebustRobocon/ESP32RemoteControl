#include "FT6336.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "hal/gpio_types.h"
#include "hal/i2c_types.h"
#include <stdint.h>

#define TAG "FT6336"
#define FT6336_I2C_ADDR 0x38

esp_err_t FT6636_init(void)
{
    esp_err_t ret;

    gpio_config_t RST_Cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << CTP_RST,
    };

    ret = gpio_config(&RST_Cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "GPIO_RST配置失败: %d\r\n", ret);
        return ret;
    }

    gpio_set_level(CTP_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(CTP_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "GPIO 初始化成功\r\n");

    i2c_config_t I2C_Config = {
        .mode = I2C_MODE_MASTER,
        .scl_io_num = CTP_SCL,
        .sda_io_num = CTP_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000};

    ret = i2c_param_config(I2C_NUM_0, &I2C_Config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C 参数配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C 驱动安装失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C初始化成功\r\n");

    uint8_t test_cmd = 0x00; // 寄存器 0x00：芯片状态
    uint8_t test_recv = 0;

    ret = i2c_master_write_read_device(I2C_NUM_0,
                                       FT6336_I2C_ADDR,
                                       &test_cmd, 1,
                                       &test_recv, 1,
                                       50 / portTICK_PERIOD_MS);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "FT6336 不响应, I2C 通信失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "FT6336初始化成功\r\n");

    return ESP_OK;
}

static uint8_t ft6336_read_point_num(void)
{
    uint8_t count = 0;
    esp_err_t ret;

    uint8_t reg = 0x02;

    ret = i2c_master_write_read_device(I2C_NUM_0, FT6336_I2C_ADDR, &reg, 1,
                                       &count, 1, 20 / portTICK_PERIOD_MS);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "读取触摸点数量失败");
        return 0;
    }

    return count & 0x0F;
}

static esp_err_t ft6336_read_position(uint8_t num,uint16_t *x, uint16_t *y)
{
    uint8_t buf[4];
    uint8_t reg = 0x03; // Touch2 起始寄存器
    if(num==2)
        reg = 0x09;
    esp_err_t ret;

    ret = i2c_master_write_read_device(I2C_NUM_0, FT6336_I2C_ADDR, &reg, 1,
                                       buf, 4,
                                       20 / portTICK_PERIOD_MS);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "读取触摸点2失败");
        return ret;
    }

    uint16_t xh = buf[0] & 0x0F;
    uint16_t xl = buf[1];
    uint16_t yh = buf[2] & 0x0F;
    uint16_t yl = buf[3];

    *x = (xh << 8) | xl;
    *y = (yh << 8) | yl;

    // 限制范围（竖屏）
    if (*x >= 240)
        *x = 239;
    if (*y >= 320)
        *y = 319;

    return ESP_OK;
}

int ft6336_update_touch(FT6336_Touch_t *touch_info)
{
    int count = ft6336_read_point_num();
    touch_info->touch_count = count;
    if (count == 1)
    {
        uint16_t x, y;
        esp_err_t ret = ft6336_read_position(1,&x, &y);
        if (ret != ESP_OK)
            return 0;
        touch_info->x1 = x;
        touch_info->y1 = y;
        return 1;
    }
    else if (count >= 2)
    {
        uint16_t x1, y1, x2, y2;
        esp_err_t ret;

        ret = ft6336_read_position(1,&x1, &y1);
        if (ret != ESP_OK)
            return 0;

        ret = ft6336_read_position(2,&x2, &y2);
        if (ret != ESP_OK)
            return 1;

        touch_info->x1 = x1;
        touch_info->y1 = y1;
        touch_info->x2 = x2;
        touch_info->y2 = y2;

        return 2;
    }
    else
        return 0;
}
