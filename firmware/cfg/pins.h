/**
 *    cfg/pins.h
 *
 *    Handy global defines for pin/function mapping
 *
 **/

#ifndef PICOPOST_DEF_PINS
#define PICOPOST_DEF_PINS

#if PICOPOST_BOARD_ID == PICOPOST_BOARD_TYPE_STD_RP2040
#include "pins-std.h"
#else
#if PICOPOST_BOARD_ID == PICOPOST_BOARD_TYPE_PRO_RP2350
#include "pins-pro.h"
#endif // PICOPOST_BOARD_ID == PICOPOST_BOARD_TYPE_PRO_RP2350
#endif // PICOPOST_BOARD_ID == PICOPOST_BOARD_TYPE_STD_RP2040

#define GPIOEXP_KEY_UP 0
#define GPIOEXP_KEY_DOWN 3
#define GPIOEXP_KEY_SELECT 2
#define GPIOEXP_KEY_BACK 1
#define GPIOEXP_OUT_BUZZER 4
#define GPIOEXP_IN_DISPTYPE 5
#define GPIOEXP_IN_DISPSIZE 6
#define GPIOEXP_IN_DISPROT 7

// Pin direction mask
#define GPIOEXP_CFG_PINDIR 0b11101111

// Pin polarity mask
#define GPIOEXP_CFG_PINPOL 0b11111111

// Pin IRQ source mask
#define GPIOEXP_CFG_PINIRQ 0b00001111

#endif // PICOPOST_DEF_PINS
