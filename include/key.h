#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

void    Key_Init(void);
void    Key_Scan(void);        /* 每 1ms 调一次（在 SysTick_Handler 里） */
uint8_t Key_GetState(void);    /* 消抖后的按住状态：1=左 2=右 3=切页 0=无 */
uint8_t Key_GetEvent(void);    /* 取走一次"按下"事件，无事件返回 0 */

#endif
