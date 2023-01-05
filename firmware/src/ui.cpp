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
    { PS_Port80Reader, "Port 80h std" },
    { PS_Port84Reader, "Port 84h CPQ" },
    { PS_Port90Reader, "Port 90h PS/2" },
    { PS_Port300Reader, "Port 300h EISA" },
    { PS_Port378Reader, "Port 378h Oli" },
    { PS_VoltageMonitor, "Voltage rails" },
    { PS_Info, "Info" }
};

UserInterface::UserInterface(OLED* display, Size dispSize)
{
    this->display = display;
    this->dispSize = dispSize;
    if (this->dispSize == Size::W128xH64) {
        displayHeight = 64;
    }

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
        fillRect(display, 0, 0, 127, 8);
        drawText(display, font_8x8, content, 1, 1, WriteMode::SUBTRACT);
        display->addBitmapImage(127 - bmp_back.width, 0,
            bmp_back.width, bmp_back.height, bmp_back.image, WriteMode::SUBTRACT);
        display->sendBuffer();
    }
}

void UserInterface::DrawFooter(OLEDLine content)
{
    if (display != nullptr) {
        fillRect(display, 0, 12, 127, displayHeight - 1, WriteMode::SUBTRACT);
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
        const size_t itemHeight = 10;
        const size_t itemPadding = 2;
        size_t maxItems = displayHeight / itemHeight;
        const size_t centerAnchorY = (displayHeight - itemHeight) >> 1;
        if (maxItems % 2 == 0) {
            maxItems--;
        }
        const int minIdx = index - (maxItems >> 1);
        const int maxIdx = index + (maxItems >> 1);

        display->clear();

        int anchorOffset = -(maxItems >> 1) * itemHeight;
        for (int i = minIdx; i <= maxIdx; i++) {
            if (i >= 0 && i < currentMenu.size()) {
                if (i == index) {
                    fillRect(display, 0, centerAnchorY + anchorOffset, 116, centerAnchorY + anchorOffset + itemHeight);
                    drawText(display, font_8x8, currentMenu.at(i).second,
                        2, centerAnchorY + anchorOffset + itemPadding,
                        WriteMode::SUBTRACT);
                } else {
                    drawText(display, font_8x8, currentMenu.at(i).second,
                        2, centerAnchorY + anchorOffset + itemPadding);
                }
            }
            anchorOffset += itemHeight;
        }

        const size_t iconAnchor = (displayHeight - bmp_select.height - 1) >> 1;
        display->addBitmapImage(127 - bmp_select.width, iconAnchor,
            bmp_select.width, bmp_select.height, bmp_select.image);
        if (index > 0) {
            display->addBitmapImage(127 - bmp_arrowDown.width, iconAnchor - bmp_arrowUp.height - 3,
                bmp_arrowUp.width, bmp_arrowUp.height, bmp_arrowUp.image);
        }
        if (index < currentMenu.size() - 1) {
            display->addBitmapImage(127 - bmp_arrowDown.width, iconAnchor + bmp_arrowDown.height + 3,
                bmp_arrowDown.width, bmp_arrowDown.height, bmp_arrowDown.image);
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
            const size_t bottomOffsetSmall = displayHeight - 1 - 13;
            const size_t bottomOffsetLarge = bottomOffsetSmall - 4;
            const size_t centerOffsetSmall = bottomOffsetSmall - 16;
            const size_t centerOffsetLarge = centerOffsetSmall - 4;
            const size_t topOffsetSmall = centerOffsetSmall - 16;
            const size_t topOffsetLarge = topOffsetSmall - 4;
            
            bool refresh = true;
            switch (buffer->operation) {

            case QO_Greetings: {
                drawText(display, font_12x16, "PicoPOST", 1, 1);
                drawLine(display, 0, 18, 128, bottomOffsetSmall);
                drawText(display, font_8x8, PROJ_STR_VER, 127 - (strlen(PROJ_STR_VER) * 8), 22);
            } break;

            case QO_Volts: {
                fillRect(display, 0, 9, 127, displayHeight - 1, WriteMode::SUBTRACT);
                if (displayHeight == 32) {
                    sprintf(textBuffer[0], "%01.1f", buffer->volts5);
                    sprintf(textBuffer[1], "%02.1f", buffer->volts12);
                    sprintf(textBuffer[2], "%02.1f", buffer->voltsN12);

                    drawText(display, font_5x8, "+5V", 2, 9);
                    drawText(display, font_8x8, textBuffer[0], 2, bottomOffsetSmall);

                    drawText(display, font_5x8, "+12V", 42, 9);
                    drawText(display, font_8x8, textBuffer[1], 42, bottomOffsetSmall);

                    drawText(display, font_5x8, "-12V", 82, 9);
                    drawText(display, font_8x8, textBuffer[2], 82, bottomOffsetSmall);
                } else if (displayHeight == 64) {
                    sprintf(textBuffer[0], "%01.2f", buffer->volts5);
                    sprintf(textBuffer[1], "%02.2f", buffer->volts12);
                    sprintf(textBuffer[2], "%02.2f", buffer->voltsN12);

                    drawText(display, font_5x8, "+5V", 4, 11);
                    drawText(display, font_8x8, textBuffer[0], 4, 23);

                    drawText(display, font_5x8, "+12V", 67, 11);
                    drawText(display, font_8x8, textBuffer[1], 67, 23);

                    drawText(display, font_5x8, "-12V", 46, 37);
                    drawText(display, font_8x8, textBuffer[2], 46, 49);
                }
            } break;

            case QO_P80Data:
            case QO_P80Reset: {
                HistoryShift();
                fillRect(display, 0, 12, 127, displayHeight - 1, WriteMode::SUBTRACT);
                if (buffer->operation == QO_P80Reset) {
                    sprintf(textBuffer[0], "R!");
                } else {
                    sprintf(textBuffer[0], "%02X", buffer->data);
                }
                const size_t itemSpace = 24;
                const size_t vertOffset = (displayHeight == 64) ? topOffsetSmall : bottomOffsetSmall;
                size_t horzOffset = 99;
                for (size_t idx = 0; idx < 5; idx++) {
                    drawText(display, (idx == 0) ? font_12x16 : font_8x8,
                        textBuffer[idx], horzOffset, (idx == 0) ? vertOffset - 4 : vertOffset );
                    horzOffset -= itemSpace;
                }
/*
                if (displayHeight == 32) {
                    drawText(display, font_8x8, textBuffer[4], 0, bottomOffsetSmall);
                    drawText(display, font_8x8, textBuffer[3], 23, bottomOffsetSmall);
                    drawText(display, font_8x8, textBuffer[2], 47, bottomOffsetSmall);
                    drawText(display, font_8x8, textBuffer[1], 71, bottomOffsetSmall);
                    drawText(display, font_12x16, textBuffer[0], 96, bottomOffsetLarge);
                } else if (displayHeight == 64) { 
                    drawText(display, font_8x8, textBuffer[4], 0, topOffsetSmall);
                    drawText(display, font_8x8, textBuffer[3], 23, topOffsetSmall);
                    drawText(display, font_8x8, textBuffer[2], 47, topOffsetSmall);
                    drawText(display, font_8x8, textBuffer[1], 71, topOffsetSmall);
                    drawText(display, font_12x16, textBuffer[0], 96, topOffsetLarge);

                    // Placeholder, printing post code description
                    // fetch code meaning!
                    size_t lineOffset = topOffsetLarge + 16;
                    const size_t lineHeight = 7;
                    // 25 char limit per line!!!
                    drawText(display, font_5x8, "Farts are farting within", 2, lineOffset);
                    lineOffset += lineHeight;
                    drawText(display, font_5x8, "the farted farter. Do not", 2, lineOffset);
                    lineOffset += lineHeight;
                    drawText(display, font_5x8, "smell.", 2, lineOffset);
                    lineOffset += lineHeight;
                    drawText(display, font_5x8, "And another one", 2, lineOffset);
                }
*/
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
            printf("%10.3f | 5 V @ %.2f | 12 V @ %.2f | -12 V @ %.2f\n",
                tstamp, buffer->volts5, buffer->volts12, buffer->voltsN12);
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

const MenuEntry UserInterface::GetMenuEntry(uint index)
{
    if (index < currentMenu.size()) {
        return currentMenu.at(index);
    } else {
        return { PS_MAX_PROG, "" };
    }
}

void UserInterface::HistoryShift()
{
    for (uint i = MAX_HISTORY - 1; i > 0; i--) {
        memcpy(textBuffer[i], textBuffer[i - 1], MAX_VALUE_LEN);
    }
}
