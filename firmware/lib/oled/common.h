/**
 *    oled/common.h
 *
 *    Free adaptation of the oled_i2c example project from pico-examples.
 *    Original source here: https://github.com/raspberrypi/pico-examples/tree/master/i2c/oled_i2c
 *
 */

#ifndef __PICOPOST_OLED_COM
#define __PICOPOST_OLED_COM

#include "hardware/i2c.h"
#include "proj.h"
#include "fonts.h"

void OLED_Init();
void OLED_SetContrast(uint8_t);
void OLED_ClearScreen();
void OLED_PrintLine(const char* _text, OLED_Font _font, uint8_t _line);

#endif // __PICOPOST_OLED_COM
