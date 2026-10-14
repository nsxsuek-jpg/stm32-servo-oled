#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f10x.h"

void Buzzer_Init(void);
void Buzzer_Beep(uint16_t ms);   /* 非阻塞：响 ms 毫秒后由 Buzzer_Scan 关闭 */
void Buzzer_Scan(void);          /* 每 1ms 调一次（在 SysTick_Handler 里） */
void Buzzer_Off(void);           /* 立即停 */

/* 自检用：1.5 秒一个周期，先给高 1.2 秒、再给低 0.3 秒。
   故意做成不对称，靠"响长还是响短"就能反推出触发极性：
     听到【长响 + 短停】 -> 高电平触发 -> BUZZER_ACTIVE_LOW 用 0
     听到【短响 + 长停】 -> 低电平触发 -> BUZZER_ACTIVE_LOW 用 1
   完全听不到变化(一直响/一直不响) -> 信号线没接在 B0 上，是接线问题 */
void Buzzer_TestPattern(void);

#endif
