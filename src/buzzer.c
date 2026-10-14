#include "buzzer.h"
#include "delay.h"

/* ============================================================
 * 蜂鸣器触发极性
 *   0 = 高电平响（多数"三针模块"，I/O 给高电平导通三极管）
 *   1 = 低电平响（部分模块是低电平触发）
 *
 *   实测结果：第 4 页输出 1.2 秒高 + 0.3 秒低，听到的是【短响】
 *   -> 响的那 0.3 秒对应低电平 -> 本模块是低电平触发（已按此设置）。
 *
 *   怎么确定？翻到第 4 页自检页，那里会输出 1.2 秒高 + 0.3 秒低：
 *     听到【长响 + 短停】 -> 用 0
 *     听到【短响 + 长停】 -> 用 1
 *     一直是响的 / 一直不响 -> 跟极性无关，是接线问题
 * ============================================================ */
#define BUZZER_ACTIVE_LOW   1

#if BUZZER_ACTIVE_LOW
  #define BUZZER_ON()    GPIO_ResetBits(GPIOB, GPIO_Pin_0)
  #define BUZZER_OFF()   GPIO_SetBits(GPIOB, GPIO_Pin_0)
#else
  #define BUZZER_ON()    GPIO_SetBits(GPIOB, GPIO_Pin_0)
  #define BUZZER_OFF()   GPIO_ResetBits(GPIOB, GPIO_Pin_0)
#endif

static volatile uint32_t s_beep_end = 0;
static volatile uint8_t  s_beeping  = 0;

void Buzzer_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    BUZZER_OFF();           /* 初始不响 */
    s_beeping = 0;
}

/* 非阻塞：这里只负责"开"，"关"交给 Buzzer_Scan 按时间做 */
void Buzzer_Beep(uint16_t ms)
{
    BUZZER_ON();
    s_beep_end = GetTick() + ms;
    s_beeping  = 1;
}

void Buzzer_Scan(void)
{
    if (s_beeping && (int32_t)(GetTick() - s_beep_end) >= 0) {
        BUZZER_OFF();
        s_beeping = 0;
    }
}

void Buzzer_Off(void)
{
    BUZZER_OFF();
    s_beeping = 0;
}

/* 自检波形：直接翻转引脚，不经过极性宏。
   1.5 秒一个周期：高 1.2 秒 + 低 0.3 秒。 */
void Buzzer_TestPattern(void)
{
    if ((GetTick() % 1500U) < 1200U) {
        GPIO_SetBits(GPIOB, GPIO_Pin_0);
    } else {
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);
    }
}
