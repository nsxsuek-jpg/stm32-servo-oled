#ifndef __LED_H
#define __LED_H

#include "stm32f10x.h"

void LED_Init(void);
void LED_SetPage(uint8_t page);   /* page = 1/2/3 */

#endif