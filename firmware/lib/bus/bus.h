/**
  *    lib/bus/bus.h
  * 
  *    Declare how we deal with the ISA bus!
  * 
  **/

#ifndef __PICOPOST_SRC_BUS
#define __PICOPOST_SRC_BUS

#include "pins.h"

#include "hardware/gpio.h"
#include <stdint.h>

void Bus_Init();

uint8_t Bus_IsResetting();
uint8_t Bus_IsReady();

uint8_t Bus_GetAddressUpper();
uint8_t Bus_GetAddressLower();
uint16_t Bus_GetAddress();
uint8_t Bus_GetData();

#endif // __PICOPOST_SRC_BUS