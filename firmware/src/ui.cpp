#include "ui.hpp"
#include "bitmaps.hpp"
#include "hardware/gpio.h"
#include "pins.h"
#include "proj.h"
#include "shapeRenderer/ShapeRenderer.h"
#include "textRenderer/TextRenderer.h"
#include <stdio.h>

using namespace pico_oled;

const std::vector<MenuEntry> UserInterface::s_mainMenu = {
    { PS_Port80Reader, "Port 80h" },
    { PS_VoltageMonitor, "Voltage rails" },
    { PS_Info, "Info" }
};

UserInterface::UserInterface(OLED* display, Size dispSize)
{
    this->display = display;
    this->dispSize = dispSize;

    this->currentMenu = s_mainMenu;
}

void UserInterface::ClearScreen()
{
    if (display != nullptr) {
        display->clear();
        display->sendBuffer();
    }
}

void UserInterface::DrawHeader(OLEDLine content)
{
    if (display != nullptr) {
        fillRect(display, 0, 0, 127, 10, WriteMode::SUBTRACT);
        drawText(display, font_8x8, content, 1, 0);
        drawLine(display, 0, 10, 90, 10);
        display->sendBuffer();
    }
}

void UserInterface::DrawFooter(OLEDLine content)
{
    if (display != nullptr) {
        fillRect(display, 0, 12, 127, 31, WriteMode::SUBTRACT);
        drawText(display, font_8x8, content, 2, 18);
        display->sendBuffer();
    }
}

void UserInterface::SetMenuContext(const std::vector<MenuEntry>& menu)
{
    currentMenu = menu;
}

void UserInterface::DrawMenu(uint index)
{
    if (display != nullptr) {
        display->clear();

        if (index > 0) {
            drawText(display, font_8x8, currentMenu.at(index - 1).second, 2, 2);
            display->addBitmapImage(118, 0, bmp_arrowUp.width,
                bmp_arrowUp.height, bmp_arrowUp.image);
        }

        drawText(display, font_8x8, currentMenu.at(index).second, 2, 12);
        drawRect(display, 0, 10, 116, 20);

        if (index < PS_MAX_PROG - 1) {
            drawText(display, font_8x8, currentMenu.at(index + 1).second, 2, 22);
            display->addBitmapImage(118, 23, bmp_arrowDown.width,
                bmp_arrowDown.height, bmp_arrowDown.image);
        }

        display->sendBuffer();
    }
}

void UserInterface::NewData(const QueueData* buffer)
{
    if (buffer != nullptr) {
        double tstamp = buffer->timestamp / 1000.0;

        // OLED data handler
        if (display != nullptr) {
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

        // USB ACM serial output
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

void UserInterface::ClearBuffers()
{
    memset(textBuffer, '\0', sizeof(textBuffer));
}

void UserInterface::HistoryShift()
{
    for (uint i = MAX_HISTORY - 1; i > 0; i--) {
        memcpy(textBuffer[i], textBuffer[i - 1], MAX_VALUE_LEN);
    }
}
