#ifndef __FT6336_H_
#define __FT6336_H_


#include "esp_err.h"

#define SD_CS 3
#define CTP_INT 9
#define CTP_SDA 10
#define CTP_RST 11
#define CTP_SCL 12

typedef struct
{
    uint8_t touch_count; // 触摸点数量
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
} FT6336_Touch_t;


esp_err_t FT6636_init(void);
int ft6336_update_touch(FT6336_Touch_t *touch_info);

#endif