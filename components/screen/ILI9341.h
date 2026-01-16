#ifndef __ILI9341_H__
#define __ILI9341_H__

#include <stdint.h>

/*--------------------包类型----------------------------*/
#define PACK_TYPE_CMD       0
#define PACK_TYPE_DATA      1

/* -------------------- TFT 屏幕参数 -------------------- */
#define TFT_WIDTH              240
#define TFT_HEIGHT             320

/* -------------------- TFT SPI 管脚定义 ----------------- */
#define TFT_PIN_NUM_MISO 13
#define TFT_PIN_NUM_MOIS 21
#define TFT_PIN_NUM_SCK  14
#define LCD_RS  47          //液晶屏命令/数据选择控制（高：数据/低:命令）DC
#define LCD_RST 48          //液晶屏复位控制信号，低电平复位
#define LCD_CS  45          //液晶屏片选控制信号，低电平有效


void ILI9341_Init();
void color_data_conversion(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2,uint16_t *pixel_data_src);
void ILI9341_FreshScreen(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);
uint8_t wait_spi_trans_finished();

#endif
