#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f10x.h"

void     Delay_Init(void);        /* 把 SysTick 配成 1ms 节拍 */
void     Delay_TickInc(void);     /* 只给 SysTick_Handler 调用 */
uint32_t GetTick(void);           /* 自 Delay_Init 起经过的毫秒数（溢出安全） */
void     Delay_ms(uint32_t ms);   /* 基于节拍的阻塞延时，比忙等准确 */

#endif
