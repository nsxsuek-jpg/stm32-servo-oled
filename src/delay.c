#include "delay.h"

static volatile uint32_t s_tick = 0;
static volatile uint8_t  s_running = 0;

void Delay_Init(void)
{
    /* SystemCoreClock 必须已经被 SystemClock_Config() 更新过 */
    if (SysTick_Config(SystemCoreClock / 1000U) == 0U) {
        s_running = 1;
    }
    /* 万一 SystemCoreClock 异常，也绝不 while(1) 卡死：
       让 Delay_ms() 走忙等兜底分支，至少系统还能跑 */
}

void Delay_TickInc(void)
{
    s_tick++;
}

uint32_t GetTick(void)
{
    return s_tick;
}

void Delay_ms(uint32_t ms)
{
    uint32_t start;

    if (s_running == 0U) {
        /* 节拍还没起来（例如在 Delay_Init 之前调用）：退回忙等 */
        while (ms--) {
            for (volatile uint32_t i = 0; i < 7200; i++);
        }
        return;
    }

    start = GetTick();
    while ((uint32_t)(GetTick() - start) < ms);
}
