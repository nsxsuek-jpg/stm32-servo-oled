#include "adc.h"

#define ADC_FULLSCALE       4095u     /* 12 位 ADC */
#define ADC_VREFINT_MV       1200u    /* 内部基准 VREFINT 典型值 1.20V */
#define ADC_DEFAULT_VDD_MV   3300u    /* 读不到 VREFINT 时的兜底值 */
#define ADC_TIMEOUT_LOOPS    1000000UL /* 转换等待超时，避免死等卡死 */

/* 自检采样次数：每次转换约 0.5us，60000 次约 28ms，
   足够覆盖一个 20ms 的舵机 PWM 周期（取到高电平和低电平各若干次）*/
#define ADC_SELFTEST_SAMPLES  60000UL

/* 最近一次测得的供电电压：ADC_GetVoltage_mV() 用它做参考 */
static uint16_t s_vdd_mV = ADC_DEFAULT_VDD_MV;

/* 启动一次转换并取结果；超时返回 0 */
static uint8_t ADC_ConvertOnce(uint16_t *value)
{
    uint32_t timeout = ADC_TIMEOUT_LOOPS;

    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET) {
        if (--timeout == 0) {
            return 0;                       /* 不等死，交给调用者处理 */
        }
    }
    *value = ADC_GetConversionValue(ADC1);  /* 读 DR 会顺带清 EOC */
    return 1;
}

/* 切通道 -> 丢弃第一次转换结果 -> 取 samples 次平均
   （ST 建议：切换通道后的第一次转换不可信） */
static uint16_t ADC_ReadChannel(uint8_t channel, uint8_t sample_time, uint8_t samples)
{
    uint32_t sum = 0;
    uint16_t value = 0;

    ADC_RegularChannelConfig(ADC1, channel, 1, sample_time);

    if (!ADC_ConvertOnce(&value)) return 0;         /* 第一次：丢弃 */

    for (uint8_t i = 0; i < samples; i++) {
        if (!ADC_ConvertOnce(&value)) return 0;
        sum += value;
    }
    return (uint16_t)(sum / samples);
}

void MyADC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef  ADC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);        /* ADCCLK = 72MHz / 6 = 12MHz */

    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);

    ADC_Cmd(ADC1, ENABLE);
    ADC_TempSensorVrefintCmd(ENABLE);   /* 打开内部温度传感器和 VREFINT 通道 */

    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));

    ADC_GetVDD_mV();                    /* 先测一次 VDD 并缓存 */
}

/* 用 VREFINT 反推 VDD：VDD = 1.20V * 4095 / VREFINT 采样值 */
uint16_t ADC_GetVDD_mV(void)
{
    uint16_t vref = ADC_ReadChannel(ADC_Channel_17, ADC_SampleTime_239Cycles5, 4);

    if (vref != 0) {
        s_vdd_mV = (uint16_t)((uint32_t)ADC_VREFINT_MV * ADC_FULLSCALE / vref);
    }
    return s_vdd_mV;
}

/* 电位器电压：8 次平均，用实测 VDD 换算（原来的 3300 是写死的，
   实际 VDD 不是 3.300V 时读数就偏） */
uint16_t ADC_GetVoltage_mV(void)
{
    uint16_t raw = ADC_ReadChannel(ADC_Channel_0, ADC_SampleTime_55Cycles5, 8);

    return (uint16_t)((uint32_t)raw * s_vdd_mV / ADC_FULLSCALE);
}

/* 自检专用：密集采样 PA0，给出窗口内的最小/最大电压。
   把 PB6 跳线接到 PA0 时：
     有 PWM          -> MIN≈0.00V, MAX≈3.30V
     一直是低电平     -> MIN=MAX≈0.00V
     一直是高电平     -> MIN=MAX≈3.30V
   采样窗口跨过整个 50Hz 周期，所以不依赖采样相位，判据很硬。 */
void ADC_GetPulseRange_mV(uint16_t *min_mV, uint16_t *max_mV)
{
    uint16_t value;
    uint16_t vmin = ADC_FULLSCALE;
    uint16_t vmax = 0;
    uint32_t i;

    /* 采样源是芯片自己的推挽输出，阻抗极低，用最短采样时间即可 */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_1Cycles5);

    for (i = 0; i < ADC_SELFTEST_SAMPLES; i++) {
        if (!ADC_ConvertOnce(&value)) {
            break;
        }
        if (value < vmin) vmin = value;
        if (value > vmax) vmax = value;
    }

    if (vmin > vmax) {          /* 一次都没采到 */
        vmin = 0;
        vmax = 0;
    }

    *min_mV = (uint16_t)((uint32_t)vmin * s_vdd_mV / ADC_FULLSCALE);
    *max_mV = (uint16_t)((uint32_t)vmax * s_vdd_mV / ADC_FULLSCALE);
}
