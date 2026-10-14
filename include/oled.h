#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowFloat(uint8_t Line, uint8_t Column, float Number,
                    uint8_t IntLen, uint8_t DecLen);

/* A 页波形用的显存接口 */
void OLED_ClearGRAM(void);
void OLED_DrawPoint(uint8_t x, uint8_t y);
void OLED_Refresh(void);

#endif