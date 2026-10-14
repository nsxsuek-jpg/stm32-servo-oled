#include "servo.h"

/* ============================================================
 * 平滑运动
 *   按键只改"目标角度"，真正的输出由 1ms 节拍连续向目标逼近，
 *   所以舵机是滑过去的。原来每 50ms 直接跳 5 度，看起来就像时钟走针。
 *
 *   角度统一用"0.1 度"为单位的整数存，避免在中断里做浮点运算。
 * ============================================================ */
#define SLEW_TENTHS_PER_MS   4      /* 每 ms 追 0.4 度 -> 400 度/秒 */
#define HOLD_TENTHS_PER_MS   1      /* 按住时目标每 ms 走 0.1 度 -> 100 度/秒 */
#define ANGLE_MAX_TENTHS     1800   /* 180.0 度 */

static volatile int16_t s_target  = 900;   /* 目标角度 x10 */
static          int16_t s_current = 900;   /* 当前输出角度 x10 */
static volatile int8_t  s_hold    = 0;     /* -1 左 / 0 松开 / +1 右 */
static          uint16_t s_ccr    = 0;

/* 角度(0.1度) -> 脉宽：500us + 角度/180*2000us = 500 + t*10/9 */
static uint16_t Servo_TenthsToCcr(int16_t tenths)
{
    return (uint16_t)(500 + ((int32_t)tenths * 10) / 9);
}

/* 同一个脉宽写到 TIM4 的四个通道（PB6/PB7/PB8/PB9）*/
static void Servo_WritePulse(uint16_t pulse)
{
    TIM_SetCompare1(TIM4, pulse);
    TIM_SetCompare2(TIM4, pulse);
    TIM_SetCompare3(TIM4, pulse);
    TIM_SetCompare4(TIM4, pulse);
}

/* TIM4 挂在 APB1 上：
     APB1 分频系数 = 1  -> 定时器时钟 = PCLK1
     APB1 分频系数 > 1  -> 定时器时钟 = PCLK1 x 2
   用实际时钟算预分频，这样不管系统跑在 72MHz、64MHz 还是兜底的 8MHz，
   舵机 PWM 都是标准的 50Hz。 */
static uint32_t Servo_GetTimerClock(void)
{
    static const uint8_t apb_presc[8] = {1, 1, 1, 1, 2, 4, 8, 16};
    uint32_t ppre1  = (RCC->CFGR & RCC_CFGR_PPRE1) >> 8;
    uint32_t pclk1  = SystemCoreClock / apb_presc[ppre1];

    return (apb_presc[ppre1] == 1U) ? pclk1 : (pclk1 * 2U);
}

void Servo_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_OCInitTypeDef       TIM_OCInitStructure;
    uint32_t tim_clk;
    uint16_t psc;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* TIM4 四个通道一起输出同样的 PWM：
         CH1 = PB6（默认接舵机）  CH2 = PB7  CH3 = PB8  CH4 = PB9
       四个脚都能驱动舵机，某个插针/杜邦线接触不良时换个脚就能验证 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 计数时钟 1MHz -> 1 个计数 = 1us，ARR = 20000 正好 20ms = 50Hz */
    tim_clk = Servo_GetTimerClock();
    psc = (uint16_t)(tim_clk / 1000000U);
    if (psc == 0U) {
        psc = 1U;
    }
    psc = (uint16_t)(psc - 1U);

    TIM_InternalClockConfig(TIM4);
    TIM_TimeBaseInitStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Period            = 20000 - 1;
    TIM_TimeBaseInitStructure.TIM_Prescaler         = psc;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseInitStructure);

    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 1500;

    TIM_OC1Init(TIM4, &TIM_OCInitStructure);
    TIM_OC2Init(TIM4, &TIM_OCInitStructure);
    TIM_OC3Init(TIM4, &TIM_OCInitStructure);
    TIM_OC4Init(TIM4, &TIM_OCInitStructure);

    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_OC3PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_OC4PreloadConfig(TIM4, TIM_OCPreload_Enable);

    TIM_Cmd(TIM4, ENABLE);

    /* 上电就停在 90 度 */
    s_target  = 900;
    s_current = 900;
    s_hold    = 0;
    s_ccr     = Servo_TenthsToCcr(900);
    Servo_WritePulse(s_ccr);
}

/* 1ms 节拍：按住时连续挪目标 -> 输出平滑逼近目标 -> 更新脉宽 */
void Servo_Update(void)
{
    int16_t diff;

    if (s_hold > 0) {
        s_target = (int16_t)(s_target + HOLD_TENTHS_PER_MS);
    } else if (s_hold < 0) {
        s_target = (int16_t)(s_target - HOLD_TENTHS_PER_MS);
    }
    if (s_target < 0)             s_target = 0;
    if (s_target > ANGLE_MAX_TENTHS) s_target = ANGLE_MAX_TENTHS;

    diff = (int16_t)(s_target - s_current);
    if (diff > SLEW_TENTHS_PER_MS) {
        s_current = (int16_t)(s_current + SLEW_TENTHS_PER_MS);
    } else if (diff < -SLEW_TENTHS_PER_MS) {
        s_current = (int16_t)(s_current - SLEW_TENTHS_PER_MS);
    } else {
        s_current = s_target;
    }

    if (s_ccr != Servo_TenthsToCcr(s_current)) {
        s_ccr = Servo_TenthsToCcr(s_current);
        Servo_WritePulse(s_ccr);
    }
}

void Servo_SetAngle(float Angle)
{
    int16_t t;

    if (Angle < 0.0f)   Angle = 0.0f;
    if (Angle > 180.0f) Angle = 180.0f;

    t = (int16_t)(Angle * 10.0f + 0.5f);

    __disable_irq();
    s_target = t;
    __enable_irq();
}

void Servo_Jog(float delta)
{
    int16_t t = (int16_t)(s_target + (int16_t)(delta * 10.0f));

    if (t < 0)                   t = 0;
    if (t > ANGLE_MAX_TENTHS)    t = ANGLE_MAX_TENTHS;

    __disable_irq();
    s_target = t;
    __enable_irq();
}

void Servo_SetHold(int8_t dir)
{
    if (dir > 0)      s_hold = 1;
    else if (dir < 0) s_hold = -1;
    else              s_hold = 0;
}

float Servo_GetAngle(void)
{
    return (float)s_current / 10.0f;
}

uint16_t Servo_GetPulse(void)
{
    return Servo_TenthsToCcr(s_current);
}

/* ============================================================
 * PB6 引脚自检（不需要任何外部接线、不需要万用表）
 *   把 PB6 临时切成普通推挽输出，先驱动高再驱动低，回读 IDR：
 *     两步都对 -> 引脚的输出驱动是好的
 *     有一步不对 -> 引脚被打坏，或信号线对地短路
 *   测完立刻恢复 TIM4 复用推挽，不影响 PWM。
 *   舵机的信号线不用拔（舵机输入是高阻）。
 * ============================================================ */
uint8_t Servo_PinSelfTest(void)
{
    GPIO_InitTypeDef gpio;
    uint8_t ok = 1;
    volatile uint32_t i;

    gpio.GPIO_Pin   = GPIO_Pin_6;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;

    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &gpio);

    GPIO_SetBits(GPIOB, GPIO_Pin_6);
    for (i = 0; i < 2000; i++) { }
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_6) == 0) {
        ok = 0;
    }

    GPIO_ResetBits(GPIOB, GPIO_Pin_6);
    for (i = 0; i < 2000; i++) { }
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_6) != 0) {
        ok = 0;
    }

    /* 恢复成 TIM4_CH1 复用推挽 */
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &gpio);

    return ok;
}
