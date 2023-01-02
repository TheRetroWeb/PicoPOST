#include "ui.hpp"

#include "hardware/gpio.h"
#include <stdio.h>

#include "bitmaps.hpp"
#include "pins.h"
#include "proj.h"
#include "shapeRenderer/ShapeRenderer.h"
#include "ssd1306.hpp"
#include "sh1106.hpp"
#include "textRenderer/TextRenderer.h"

using namespace pico_oled;

OLED* display = nullptr;
char textBuffer[MAX_HISTORY][MAX_VALUE_LEN] = { '\0' };

static const char menuItem[PS_MAX_PROG][15] = {
    { "Port 80h" },
    { "Voltage rails" },
    { "Info" }
};

void HistoryShift()
{
    for (uint i = MAX_HISTORY - 1; i > 0; i--) {
        memcpy(textBuffer[i], textBuffer[i - 1], MAX_VALUE_LEN);
    }
}

void UI_PrintSerial(QueueData* buffer)
{
    if (buffer != nullptr) {
        double tstamp = buffer->timestamp / 1000.0;

        switch (buffer->operation) {

        case QO_Greetings: {
            printf("-- PicoPOST " PROJ_STR_VER " --\n");
            printf("%s\n", creditsLine);
        } break;

        case QO_Volts: {
            printf("%10.3f | 5 V @ %.2f | 12 V @ %.2f\n",
                tstamp, buffer->volts5, buffer->volts12);
        } break;

        case QO_P80Data: {
            printf("%10.3f | %02X @ %04Xh\n",
                tstamp, buffer->data, buffer->address);
        } break;

        case QO_P80Reset: {
            printf("Reset!\n");
        } break;

        default: {
            // do nothing
        } break;
        }
    }
}

void UI_InitOLED(i2c_inst_t* busInstance, uint clockRate, uint8_t address, uint8_t displayType, uint8_t displaySize)
{
    // Init I2C bus for OLED display
    i2c_init(busInstance, clockRate);
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_I2C_SDA);
    gpio_pull_up(PIN_I2C_SCL);

    // Init OLED display
    auto dispSize = Size::W128xH32;
    switch (displaySize) {
        default: {
            dispSize = Size::W128xH32;
        } break;

        case 1: {
            dispSize = Size::W128xH64;
        } break;
    }

    if (displayType == 0) {
        display = new SSD1306(busInstance, address, dispSize);
    } else if (displayType == 1) {
        display = new SH1106(busInstance, address, dispSize);
    }
}

void UI_DataOLED(QueueData* buffer)
{
    if (buffer != nullptr) {
        if (display == nullptr) {
            panic("FATAL: Can't draw to uninitialized display!");
        } else {
            bool refresh = true;
            switch (buffer->operation) {

            case QO_Greetings: {
                drawText(display, font_12x16, "PicoPOST", 1, 1);
                drawLine(display, 0, 18, 128, 18);
                drawText(display, font_8x8, PROJ_STR_VER, 127 - (strlen(PROJ_STR_VER) * 8), 22);
            } break;

            case QO_Volts: {
                fillRect(display, 0, 12, 127, 31, WriteMode::SUBTRACT);
                sprintf(textBuffer[0], "%1.2f V", buffer->volts5);
                sprintf(textBuffer[1], "%2.2f V", buffer->volts12);
                drawText(display, font_8x8, textBuffer[1], 5, 18);
                drawText(display, font_8x8, textBuffer[0], 75, 18);
            } break;

            case QO_P80Data: {
                HistoryShift();
                fillRect(display, 0, 12, 127, 31, WriteMode::SUBTRACT);
                sprintf(textBuffer[0], "%02X", buffer->data);
                drawText(display, font_8x8, textBuffer[4], 0, 18);
                drawText(display, font_8x8, textBuffer[3], 23, 18);
                drawText(display, font_8x8, textBuffer[2], 47, 18);
                drawText(display, font_8x8, textBuffer[1], 71, 18);
                drawText(display, font_12x16, textBuffer[0], 96, 14);
            } break;

            case QO_P80Reset: {
                HistoryShift();
                fillRect(display, 0, 12, 127, 31, WriteMode::SUBTRACT);
                sprintf(textBuffer[0], "R!");
                drawText(display, font_8x8, textBuffer[4], 0, 18);
                drawText(display, font_8x8, textBuffer[3], 23, 18);
                drawText(display, font_8x8, textBuffer[2], 47, 18);
                drawText(display, font_8x8, textBuffer[1], 71, 18);
                drawText(display, font_12x16, textBuffer[0], 96, 14);
            } break;

            default: {
                // do nothing
                refresh = false;
            } break;
            }

            if (refresh) {
                display->sendBuffer();
            }
        }
    }
}

void UI_SetHeaderOLED(const char* line)
{
    if (display == nullptr) {
        panic("FATAL: Can't draw to uninitialized display!");
    } else {
        fillRect(display, 0, 0, 127, 10, WriteMode::SUBTRACT);
        drawText(display, font_8x8, line, 1, 0);
        drawLine(display, 0, 10, 90, 10);
        display->sendBuffer();
    }
}

void UI_ClearScreenOLED()
{
    if (display == nullptr) {
        panic("FATAL: Can't clear uninitialized display!");
    } else {
        display->clear();
        display->sendBuffer();
    }
}

void UI_DrawMenuOLED(uint index)
{
    if (display == nullptr) {
        panic("FATAL: Can't draw to uninitialized display!");
    } else {
        display->clear();

        if (index > 0) {
            drawText(display, font_8x8, menuItem[index - 1], 2, 2);
            display->addBitmapImage(118, 0, bmp_arrowUp.width,
                bmp_arrowUp.height, bmp_arrowUp.image);
        }

        drawText(display, font_8x8, menuItem[index], 2, 12);
        drawRect(display, 0, 10, 116, 20);

        if (index < PS_MAX_PROG - 1) {
            drawText(display, font_8x8, menuItem[index + 1], 2, 22);
            display->addBitmapImage(118, 23, bmp_arrowDown.width,
                bmp_arrowDown.height, bmp_arrowDown.image);
        }

        display->sendBuffer();
    }
}

void UI_SetTextOLED(const char* line)
{
    if (display == nullptr) {
        panic("FATAL: Can't draw to uninitialized display!");
    } else {
        fillRect(display, 0, 12, 127, 31, WriteMode::SUBTRACT);
        drawText(display, font_8x8, line, 2, 18);
        display->sendBuffer();
    }
}

void UI_ClearBuffers()
{
    memset(textBuffer, '\0', sizeof(textBuffer));
}