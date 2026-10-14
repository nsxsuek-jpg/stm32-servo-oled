#ifndef __SERVO_H
#define __SERVO_H

#include "stm32f10x.h"

void    Servo_Init(void);
void    Servo_Update(void);          /* 每 1ms 调一次（在 SysTick_Handler 里）*/
void    Servo_SetAngle(float Angle); /* 设置目标角度：会平滑滑过去，不会跳变 */
void    Servo_Jog(float delta);      /* 目标角度增减（轻点一下走一步）*/
void    Servo_SetHold(int8_t dir);   /* -1 左 / 0 松开 / +1 右：按住时连续推进 */
float   Servo_GetAngle(void);        /* 当前实际角度（显示/画波形用）*/
uint16_t Servo_GetPulse(void);       /* 当前实际脉宽 us */

/* PB6 引脚自检：把引脚临时切成普通输出，驱动高/低并回读 IDR。
   返回 1 = 引脚输出驱动正常；0 = 引脚坏了或信号线对地短路。
   不需要任何外部接线，测完自动恢复 TIM4 复用功能。 */
uint8_t Servo_PinSelfTest(void);

#endif
