#include "ILI9341.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/projdefs.h"
#include "data_poll.h"

#include <stdint.h>

static spi_device_handle_t spi_handle;
static DataPoll_t kSpiTransReqPoll;

static uint16_t screen_buffer[TFT_HEIGHT*TFT_WIDTH/6] __attribute__((aligned(4))) DRAM_ATTR;

uint8_t ili9341_send_small(uint8_t *data,uint32_t size,uint8_t pack_type)
{
    if(size>4)
        return 0;
    spi_transaction_t *p_buffer=(spi_transaction_t*)PollRequireBlock(&kSpiTransReqPoll);
    if(!p_buffer)
        return 0;
    memset(p_buffer,0,sizeof(spi_transaction_t));
    p_buffer->length=8*size;
    p_buffer->cmd=pack_type;
    p_buffer->flags=SPI_TRANS_USE_TXDATA;
    for(uint8_t i=0;i<size;i++)
        p_buffer->tx_data[i]=*(data+i);
    spi_device_queue_trans(spi_handle,p_buffer,portMAX_DELAY);
    return 1;
}

uint8_t ili9341_send_large(uint8_t *data,uint32_t size,uint8_t pack_type)
{
    if(size>SPI_MAX_DMA_LEN)
        return 0;
    spi_transaction_t *p_buffer=(spi_transaction_t*)PollRequireBlock(&kSpiTransReqPoll);
    if(!p_buffer)
        return 0;
    
    memset(p_buffer,0,sizeof(spi_transaction_t));
    p_buffer->length=8*size;
    p_buffer->cmd=pack_type;
    p_buffer->flags=SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL;
    p_buffer->tx_buffer=data;
    spi_device_queue_trans(spi_handle,p_buffer,portMAX_DELAY);
    return 1;
}

static void lil9341_spi_trans_pre_cb(spi_transaction_t *trans)
{
    gpio_set_level(LCD_RS, (uint32_t)(trans->cmd));
}

static void lil9341_spi_trans_finished_cb(spi_transaction_t *trans)
{
    PollFreeBlock(&kSpiTransReqPoll,trans);
}

//SPI初始化
static esp_err_t ILI9341_Hardware_Init(void)
{
    gpio_config_t cfg = {
        .intr_type = GPIO_INTR_DISABLE,        // 禁用中断
        .mode = GPIO_MODE_OUTPUT,              // 输出模式
        .pin_bit_mask = (1ULL << LCD_RS) | (1ULL << LCD_RST),  // 引脚位掩码
        .pull_up_en = GPIO_PULLUP_DISABLE,     // 禁用上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE  // 禁用下拉
    };
    
    gpio_config(&cfg);
    gpio_set_level(LCD_RST, 1);  // 复位引脚拉高（正常工作）
    gpio_set_level(LCD_RS, 0);   // RS 初始为命令模式
    gpio_set_level(LCD_CS, 1);   // CS 初始为高（未选中）

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = TFT_PIN_NUM_MOIS,
        .miso_io_num = TFT_PIN_NUM_MISO,
        .sclk_io_num = TFT_PIN_NUM_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 320 * 4,
    };
    
    esp_err_t ret = spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK)
        return ret;

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 40000000, // 20MHz
        .mode = 0,
        .spics_io_num = LCD_CS,
        .queue_size = 32,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .clock_source = SPI_CLK_SRC_DEFAULT,
        .pre_cb = lil9341_spi_trans_pre_cb,
        .post_cb = lil9341_spi_trans_finished_cb
    };
    return spi_bus_add_device(SPI3_HOST, &dev_cfg, &spi_handle);
}


void ILI9341_Init()
{
    PollInit(&kSpiTransReqPoll, sizeof(spi_transaction_t), 32);
    ILI9341_Hardware_Init();

    gpio_set_level(LCD_RST,0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(LCD_RST,1);
    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t cmd = 0;
    uint8_t buf[5] = {0};
    /*退出休眠*/
    cmd = 0x11;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    vTaskDelay(pdMS_TO_TICKS(150)); // >=120ms，给足时间

    /*电源控制序列*/
    cmd = 0xCF;
    buf[0] = 0x00;
    buf[1] = 0x83;
    buf[2] = 0x30;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    ili9341_send_large(buf, 3, PACK_TYPE_DATA);

    cmd = 0xED;
    buf[0] = 0x64;
    buf[1] = 0x03;
    buf[2] = 0x12;
    buf[3] = 0x81;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    ili9341_send_large(buf, 4, PACK_TYPE_DATA);

    cmd = 0xE8;
    buf[0] = 0x85;
    buf[1] = 0x01;
    buf[2] = 0x79;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    ili9341_send_large(buf, 3, PACK_TYPE_DATA);

    cmd = 0xCB;
    buf[0] = 0x39;
    buf[1] = 0x2C;
    buf[2] = 0x00;
    buf[3] = 0x34;
    buf[4] = 0x02;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    ili9341_send_large(buf, 5, PACK_TYPE_DATA);

    cmd = 0xF7;
    buf[0] = 0x20;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    ili9341_send_large(buf, 1, PACK_TYPE_DATA);

    cmd = 0xEA;
    buf[0] = 0x00;
    buf[1] = 0x00;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    ili9341_send_large(buf, 2, PACK_TYPE_DATA);

    cmd = 0xC1;
    buf[0] = 0x02;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    ili9341_send_large(buf, 1, PACK_TYPE_DATA);

    cmd = 0xC5;
    buf[0] = 0x3E;
    buf[1] = 0x28;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    ili9341_send_large(buf, 2, PACK_TYPE_DATA);

    cmd = 0xC7;
    buf[0] = 0x86;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    ili9341_send_large(buf, 1, PACK_TYPE_DATA);

    cmd = 0x36;
    buf[0] = 0x48;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    ili9341_send_large(buf, 1, PACK_TYPE_DATA);

    cmd = 0x3A;
    buf[0] = 0x55;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    ili9341_send_large(buf, 1, PACK_TYPE_DATA);

    cmd = 0xB1;
    buf[0] = 0x00;
    buf[1] = 0x18;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    ili9341_send_large(buf, 2, PACK_TYPE_DATA);

    cmd = 0xB6;
    buf[0] = 0x0A;
    buf[1] = 0x82;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    ili9341_send_large(buf, 2, PACK_TYPE_DATA);

    cmd = 0x26;
    buf[0] = 0x01;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    ili9341_send_large(buf, 1, PACK_TYPE_DATA);

    cmd = 0xE0;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    uint8_t gamma_p[15] = {
        0x0F, 0x31, 0x2B, 0x0C, 0x0E,
        0x08, 0x4E, 0xF1, 0x37, 0x07,
        0x10, 0x03, 0x0E, 0x09, 0x00
    };
    ili9341_send_large(gamma_p, 15, PACK_TYPE_DATA);

    cmd = 0xE1;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    uint8_t gamma_n[15] = {
        0x00, 0x0E, 0x14, 0x03, 0x11,
        0x07, 0x31, 0xC1, 0x48, 0x08,
        0x0F, 0x0C, 0x31, 0x36, 0x0F
    };
    ili9341_send_large(gamma_n, 15, PACK_TYPE_DATA);

    /* 列地址设置 */
    cmd = 0x2A;
    buf[0] = 0x00;
    buf[1] = 0x00;
    buf[2] = 0x00;
    buf[3] = 0xEF;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    ili9341_send_large(buf, 4, PACK_TYPE_DATA);

      /* 行地址设置 */
    cmd = 0x2B;
    buf[0] = 0x00;
    buf[1] = 0x00;
    buf[2] = 0x01;
    buf[3] = 0x3F;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);
    ili9341_send_large(buf, 4, PACK_TYPE_DATA);

    /* 开启显示 */
    cmd = 0x29;
    ili9341_send_small(&cmd, 1, PACK_TYPE_CMD);

    vTaskDelay(pdMS_TO_TICKS(120));
}

uint8_t wait_spi_trans_finished()
{
    while(1)
    {
        if(PollFreeBlockNum(&kSpiTransReqPoll)==32)
            return 0;
        PollWaitEvent(&kSpiTransReqPoll,10);
    }
    return 0;
}

void color_data_conversion(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2,uint16_t *pixel_data_src)
{
    uint32_t cnt=0;
    uint16_t temp;
    for(uint16_t i=y1;i<=y2;i++)
    {
        for(uint16_t j=x1;j<=x2;j++)
        {
            temp=*(pixel_data_src+cnt);
            *(screen_buffer+cnt)=(temp>>8)|(temp<<8);
            cnt++;
        }
    }
}

void ILI9341_FreshScreen(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2)
{
    uint8_t *pixel_data=(uint8_t*)screen_buffer;
    uint32_t pixel_data_size=(x2-x1+1)*(y2-y1+1)*sizeof(uint16_t);
    uint8_t cmd;
    uint8_t data[4];
    cmd=0x2A;
    data[0]=(uint8_t)(x1>>8);
    data[1]=(uint8_t)x1;
    data[2]=(uint8_t)(x2>>8);
    data[3]=(uint8_t)x2;
    ili9341_send_small(&cmd,1,PACK_TYPE_CMD);
    ili9341_send_small(data,4,PACK_TYPE_DATA);

    cmd=0x2B;
    data[0]=(uint8_t)(y1>>8);
    data[1]=(uint8_t)y1;
    data[2]=(uint8_t)(y2>>8);
    data[3]=(uint8_t)y2;
    ili9341_send_small(&cmd,1,PACK_TYPE_CMD);
    ili9341_send_small(data,4,PACK_TYPE_DATA);

    cmd=0x2C;
    ili9341_send_small(&cmd,1,PACK_TYPE_CMD);
    uint16_t current_pack_size;
    while(pixel_data_size)
    {
        current_pack_size=pixel_data_size>SPI_MAX_DMA_LEN ? SPI_MAX_DMA_LEN : pixel_data_size;
        pixel_data_size=pixel_data_size-current_pack_size;
        ili9341_send_large(pixel_data,current_pack_size,PACK_TYPE_DATA);
        pixel_data=pixel_data+current_pack_size;
    }
}
