/**
 * @file ui.hpp
 * @brief User interface is entirely here, visual output!
 * 
 */

#ifndef PICOPOST_UI_HPP
#define PICOPOST_UI_HPP

#include "common.hpp"
#include "hardware/i2c.h"

#define MAX_HISTORY 5
#define MAX_VALUE_LEN 10

void UI_PrintSerial(QueueData* buffer);

void UI_InitOLED(i2c_inst_t* busInstance, uint clockRate, uint8_t address, uint8_t displayType = 0, uint8_t displaySize = 0);

void UI_DataOLED(QueueData* buffer);

void UI_SetHeaderOLED(const char* line);

void UI_ClearScreenOLED();

void UI_DrawMenuOLED(uint index);

void UI_SetTextOLED(const char* line);

void UI_ClearBuffers();

#endif // PICOPOST_UI_HPP