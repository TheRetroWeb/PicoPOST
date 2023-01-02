/**
 *    src/main.h
 *
 *    Some handy info for running the main application
 *
 **/

#ifndef __PICOPOST_SRC_MAIN
#define __PICOPOST_SRC_MAIN

#include <stdint.h>
#include "pico/time.h"

// Serial is slow, but I hope we can make do without filling up 9.5 kBytes of
// bus data in less than we can empty the serial buffer at 115200 bps
#define MAX_QUEUE_LENGTH 512

void Aux_UI();

void Aux_KeyDebounceArm();
int64_t Aux_KeyDebounceRelease(alarm_id_t id, void* userData);

void Aux_KeyISR(uint gpio, uint32_t event_mask);

uint8_t Aux_WaitForInput();
void Aux_ClearKeyIrq();

#endif // __PICOPOST_SRC_MAIN