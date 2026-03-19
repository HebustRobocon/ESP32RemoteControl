#include "core.h"
#include "hardware.h"
#include "comm.h"
#include "driver/adc.h"
#include "math.h"

float NormalizationRocker(int adc_value, int dead_zone, int offset);
float CalcBatteryVoltage();
void BatteryADCInit();

// 初始化LS165硬件接口
static void InitButtonsInput(void)
{
    // SH/LD + CLK = OUTPUT
    gpio_config_t cfg_out = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << SH_LD_PIN) | (1ULL << CLK_PIN),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE};
    gpio_config(&cfg_out);

    // QH = INPUT with pull-up !!!
    gpio_config_t cfg_in = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << QH_PIN),
        .pull_up_en = GPIO_PULLUP_ENABLE, // 必须打开
        .pull_down_en = GPIO_PULLDOWN_DISABLE};
    gpio_config(&cfg_in);

    // 初始状态
    gpio_set_level(CLK_PIN, 0);
    gpio_set_level(SH_LD_PIN, 1); // High = shift mode
}

// 返回按键状态
static uint32_t UpdateButtons()
{
    uint32_t temp = 0;
    gpio_set_level(SH_LD_PIN, 0); // 数据锁存
    esp_rom_delay_us(4);
    gpio_set_level(SH_LD_PIN, 1); // 数据锁存
    esp_rom_delay_us(4);
    for (int i = 0; i < 16; i++)
    {
        gpio_set_level(CLK_PIN, 1);
        esp_rom_delay_us(4); // 数据稳定时间
        temp = temp | (gpio_get_level(QH_PIN) << i);
        gpio_set_level(CLK_PIN, 0);
        esp_rom_delay_us(4);
    }
    return temp;
}

int Process(int adc_value, int dead_zone, int offset)
{
    int v = adc_value + offset; // 注意这里是 + offset
    int center = 0x7FF;         // 恒定中心点 2047

    // 死区
    if (v < center + dead_zone && v > center - dead_zone)
        return 0;

    // 计算偏移值，不进行归一化
    int out;
    if (v > center)
        out = v - center - dead_zone;
    else
        out = v - center + dead_zone;

    // 反转方向（加负号）
    return -out;
}

void default_remote_state_flush(const int *rockers, const uint16_t key,void* user_data)
{
    PackControl_t *remoteInfo = (PackControl_t *)user_data;
    static float rocker_raw_value[4];

    rocker_raw_value[0] = Process(rockers[0], 120, -20);
    rocker_raw_value[1] = Process(rockers[1], 120, -75);
    rocker_raw_value[2] = Process(rockers[2], 120, 70);
    rocker_raw_value[3] = Process(rockers[3], 120, -5);
    static float rocker_last[4] = {0};
    for (int i = 0; i < 4; i++)
    {
        rocker_raw_value[i] = 0.21f * rocker_raw_value[i] + (1.0f - 0.21f) * rocker_last[i];
        rocker_last[i] = rocker_raw_value[i];   // 更新历史值
        remoteInfo->rocker[i] = rocker_raw_value[i];
    }
    
    remoteInfo->Key = key;
    printf("update\r\n");
    asyn_comm_send_pack_nak((uint8_t *)remoteInfo, PACK_CONTROL_CMD, sizeof(PackControl_t));
}

static PackControl_t remoteInfo;
static float battery_voltage = 4.0f;
static uint16_t buttons_state=0;
static int rocker_adc_value[4] = {0};     // ADC原始数据
static RemoteStateFlush_t kRemoteStateFlushFunc = default_remote_state_flush;
void *kRemoteStateFlushUserData = &remoteInfo;
void CoreTask(void *param) // 遥控器核心任务
{
    float battery_alpha = 0.95; // 电池电压低通滤波系数
    InitButtonsInput();
    BatteryADCInit();

    adc1_config_width(ADC_WIDTH_BIT_12);                        // 配置ADC分辨率为12位
    adc1_config_channel_atten(LEFT_ROCKER_X, ADC_ATTEN_DB_12);  // 配置ADC通道衰减
    adc1_config_channel_atten(LEFT_ROCKER_Y, ADC_ATTEN_DB_12);  // 配置ADC通道衰减
    adc1_config_channel_atten(RIGHT_ROCKER_X, ADC_ATTEN_DB_12); // 配置ADC通道衰减
    adc1_config_channel_atten(RIGHT_ROCKER_Y, ADC_ATTEN_DB_12); // 配置ADC通道衰减

    TickType_t last_wake_time = xTaskGetTickCount();
    while (1)
    {
        rocker_adc_value[0] = adc1_get_raw(LEFT_ROCKER_X);  // 读取ADC值
        rocker_adc_value[1] = adc1_get_raw(LEFT_ROCKER_Y);  // 读取ADC值
        rocker_adc_value[2] = adc1_get_raw(RIGHT_ROCKER_X); // 读取ADC值
        rocker_adc_value[3] = adc1_get_raw(RIGHT_ROCKER_Y); // 读取ADC值
        buttons_state=UpdateButtons();
        //遥控器状态刷新
        kRemoteStateFlushFunc(rocker_adc_value,buttons_state,kRemoteStateFlushUserData);
        //电池电量更新
        battery_voltage = (1.0f - battery_alpha) * CalcBatteryVoltage() + battery_alpha * battery_voltage;

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(20));
    }
}

void set_remote_flush_func(RemoteStateFlush_t func,void* user_data)
{
    if(func == NULL)
    {
        kRemoteStateFlushFunc = default_remote_state_flush;
        kRemoteStateFlushUserData = &remoteInfo;
        return;
    }
    kRemoteStateFlushFunc = func;
    kRemoteStateFlushUserData = user_data;
}

RemoteStateFlush_t get_remote_flush_func()
{
    return kRemoteStateFlushFunc;
}

void get_remote_state(int rocker_raw_data[4],uint16_t* key_data)
{
    memcpy(rocker_raw_data, rocker_adc_value, sizeof(rocker_adc_value));
    *key_data = buttons_state;
}

float NormalizationRocker(int adc_value, int dead_zone, int offset)
{
    int v = adc_value + offset; // 注意这里是 + offset
    int center = 0x7FF;         // 恒定中心点 2047

    // 死区
    if (v < center + dead_zone && v > center - dead_zone)
        return 0.0f;

    // 比例系数，归一化到 -1 ~ 1
    float k = 1.0f / (0x7FF - 2 * dead_zone);

    float out;
    if (v > center)
        out = (v - center - dead_zone) * k;
    else
        out = (v - center + dead_zone) * k;

    // 限幅
    if (out > 1.0f)
        out = 1.0f;
    if (out < -1.0f)
        out = -1.0f;

    return out;
}

void BatteryADCInit()
{
    adc2_config_channel_atten(BATTERY_INPUT_CHANNEL, ADC_ATTEN_DB_12); // 测量电池电压
}
int battery_adc_raw_value = 0;
float CalcBatteryVoltage()
{
    adc2_get_raw(BATTERY_INPUT_CHANNEL, ADC_WIDTH_BIT_12, &battery_adc_raw_value);
    float voltage = battery_adc_raw_value;
    return 0.001664f * voltage + 0.2467f;
}

float get_battery_voltage()
{
    return battery_voltage;
}

float Get_Battery_level(float x)
{
    if(x>4.16)
        return 100.0f;
    if(x<3.58)
        return 0.0f;
    // 归一化
    float xn = (x - 3.861f) / 0.1846f;

    // Poly5 系数
    float p1 = -3.099f;
    float p2 = 7.096f;
    float p3 = 5.464f;
    float p4 = -21.73f;
    float p5 = 33.52f;
    float p6 = 65.03f;

    // Horner 法则计算多项式
    float y = (((((p1 * xn + p2) * xn + p3) * xn + p4) * xn + p5) * xn + p6);

    return y;
}

static TaskHandle_t buttons_scane_task_handle;
/**
 * @brief 遥控器硬件核心初始化，
 */
void RemoteCoreInit()
{
    xTaskCreate(CoreTask, "CoreTask", 4096, NULL, 5, &buttons_scane_task_handle);
}
