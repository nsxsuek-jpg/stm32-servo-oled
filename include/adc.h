#ifndef __ADC_H
#define __ADC_H

#include "stm32f10x.h"

void     MyADC_Init(void);
uint16_t ADC_GetVDD_mV(void);       /* 读内部 1.20V 基准反推供电电压，并缓存下来 */
uint16_t ADC_GetVoltage_mV(void);   /* 电位器电压：用最近一次测得的 VDD 换算 */

/* 自检专用：在约一整个 50Hz 周期内密集采样 PA0，给出最小/最大电压(mV)。
   用法：拔掉电位器在 PA0 上的线，用跳线把 PB6 接到 PA0，
         有 PWM 时 MIN≈0 而 MAX≈VDD；一直不跳变说明这个脚没有输出。 */
void     ADC_GetPulseRange_mV(uint16_t *min_mV, uint16_t *max_mV);

#endif
