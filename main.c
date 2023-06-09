#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

#define ROCKER_Y1_PIN        26         // 四路ADC引脚
#define ROCKER_X1_PIN        27
#define ROCKER_Y2_PIN        28
#define ROCKER_X2_PIN        29

#define PPM_PIN              22         // 输出PPM信号的引脚

#define CHANNEL_NUM          8


#define MS20    (20000) /* 20ms */      // PPM信号每个周期时间 20ms
#define MS05    (500)   /* 0.5ms */     // PPM每个通道时间间隔
#define PPM_NUM (CHANNEL_NUM * 2 + 2)   // PPM信号变化次数

#define ADC_VALUE_MAX    4095
#define ADC_VALUE_MIN    0
#define ADC_VALUE_MID    2047

#define MAPPED_VALUE_MAX 2000
#define MAPPED_VALUE_MIN 1000
#define MAPPED_VALUE_MID 1500

uint16_t volatile chResult[CHANNEL_NUM] = { 2048, 2048, 2048, 2048, 0, 0, 0, 0 };   // ADC输出的值
uint16_t volatile ppmValues[PPM_NUM] = { MS05, 500, MS05, 1000, MS05, 1000, MS05, 1000, MS05, 1000, MS05, 1000, MS05, 1000, MS05, 1000, MS05, 8000 };
uint16_t volatile ppmIndex = 0;
uint8_t volatile ppmLevel = 0;
uint8_t volatile chReverse[CHANNEL_NUM] = { 0, 1, 1, 0, 0, 0, 0, 0 };  // 用来控制反向，电脑模拟器软件有反向功能，可不使用
 

void ADCInit()
{
    adc_init();
    adc_gpio_init(ROCKER_Y1_PIN);
    adc_gpio_init(ROCKER_X1_PIN);
    adc_gpio_init(ROCKER_Y2_PIN);
    adc_gpio_init(ROCKER_X2_PIN);
}

void GPIOInit()
{
    gpio_init(PPM_PIN);
    gpio_set_dir(PPM_PIN, GPIO_OUT);
    gpio_put(PPM_PIN, ppmLevel);
}

float map(float value, float fromLow, float fromHigh, float toLow, float toHigh)
{
    return ((value - fromLow ) * (toHigh - toLow) / (fromHigh - fromLow) + toLow);
}

int mapChValue(int val, int lower, int middle, int upper, int reverse)
{
    if(val > upper)
        val = upper;

    if(val < lower)
        val = lower;
        
    if ( val < middle )
    {
        val = (int)map(val, lower, middle, MAPPED_VALUE_MIN, MAPPED_VALUE_MID);
    }
    else
    {
        val = (int)map(val, middle, upper, MAPPED_VALUE_MID, MAPPED_VALUE_MAX);
    }
    
    return ( reverse ? 3000 - val : val );
 }

int64_t alarmCallback(alarm_id_t id, void *user_data)
{
    gpio_put(PPM_PIN, ppmLevel);
    add_alarm_in_us(ppmValues[ppmIndex], alarmCallback, NULL, false);
    ppmLevel = !ppmLevel;
    ++ppmIndex;
    if (ppmIndex == PPM_NUM)
    {
        ppmIndex = 0;
    }

    return 0;
}

void core1_entry()
{
    gpio_put(PPM_PIN, ppmLevel);
    add_alarm_in_us(ppmValues[ppmIndex], alarmCallback, NULL, false);
    ppmLevel = !ppmLevel;
    ++ppmIndex;
}


int main()
{
    stdio_init_all();
    ADCInit();
    GPIOInit();

    uint16_t chIndex;
    uint16_t mappedValueSum = 0;
    uint16_t mappedValue = 0;

    multicore_launch_core1(core1_entry);

    while (1) {
        mappedValueSum = 0;

        for(chIndex = 0; chIndex < 4; chIndex++)
        {
            adc_select_input(chIndex);
            chResult[chIndex] = adc_read();
        }

        for(chIndex = 0; chIndex < CHANNEL_NUM; chIndex++)
        {
            mappedValue = mapChValue(chResult[chIndex], ADC_VALUE_MIN, ADC_VALUE_MID, ADC_VALUE_MAX, chReverse[chIndex]);
            ppmValues[chIndex * 2 + 1] = mappedValue - MS05;
            mappedValueSum += mappedValue;
        }

        ppmValues[PPM_NUM - 1] = MS20 - mappedValueSum;
    }
}
