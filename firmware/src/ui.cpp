#include "ui.hpp"
#include "bitmaps.hpp"
#include "hardware/gpio.h"
#include "pico/rand.h"
#include "pins.h"
#include "proj.h"
#include "shapeRenderer/ShapeRenderer.h"
#include "textRenderer/TextRenderer.h"
#include <stdio.h>
#include <stdlib.h>

using namespace pico_oled;

const std::vector<MenuEntry> UserInterface::s_mainMenu = {
    { ProgramSelect::Port80Reader, "Port 80h std" },
    { ProgramSelect::Port84Reader, "Port 84h CPQ" },
    { ProgramSelect::Port90Reader, "Port 90h PS/2" },
    { ProgramSelect::Port300Reader, "Port 300h EISA" },
    { ProgramSelect::Port378Reader, "Port 378h Oli" },
    { ProgramSelect::VoltageMonitor, "Voltage rails" },
    { ProgramSelect::Info, "Info" },
    { ProgramSelect::UpdateFW, "Update FW" }
};

UserInterface::UserInterface(OLED* display, Size dispSize)
    : display(display)
    , dispSize(dispSize)
{
    if (this->dispSize == Size::W128xH64) {
        displayHeight = 64;
    }

    this->currentMenu = s_mainMenu;
    this->spritePos.invSlope = -2.0f; // 1/m
    this->spritePos.fullyHidden = true;
    UpdateSpritePosition(spr_toaster);
}

void UserInterface::ClearScreen()
{
    if (display != nullptr) {
        display->clear();
        display->sendBuffer();
    }
}

void UserInterface::SetScreenBrightness(uint8_t level)
{
    if (display != nullptr) {
        display->setContrast(level);
    }
}

void UserInterface::DrawHeader(OLEDLine content)
{
    if (display != nullptr) {
        display->clear();
        fillRect(display, 0, 0, 127, 8);
        drawText(display, font_8x8, content, 1, 1, WriteMode::SUBTRACT);
        display->sendBuffer();
    }
}

void UserInterface::DrawFooter(OLEDLine content)
{
    if (display != nullptr) {
        fillRect(display, 0, 12, c_ui_yIconAlign - 1, displayHeight - 1, WriteMode::SUBTRACT);
        drawText(display, font_8x8, content, 2, 18);
        display->sendBuffer();
    }
}

void UserInterface::DrawFullScreen(const Icon& bmp)
{
    if (display != nullptr) {
        display->clear();
        display->addBitmapImage(0, 0, bmp.width, bmp.height, bmp.image);
        display->sendBuffer();
    }
}

void UserInterface::DrawScreenSaver(const Sprite& spr, uint8_t frameId)
{
    if (display == nullptr) return;
    if (frameId > spr.frameCount) return;

    UpdateSpritePosition(spr);

    display->clear();
    display->addBitmapImage(this->spritePos.x, this->spritePos.y, spr.width, spr.height, spr.images[frameId]);
    display->sendBuffer();
}

void UserInterface::DrawActions(const Icon& top, const Icon& middle, const Icon& bottom)
{
    if (display != nullptr) {
        const uint8_t iconAnchor = (displayHeight - c_ui_iconSize - 1) >> 1;
        display->addBitmapImage(c_ui_yIconAlign, iconAnchor + c_ui_iconSize + 3,
            bottom.width, bottom.height, bottom.image, WriteMode::INVERT);
        display->addBitmapImage(c_ui_yIconAlign, iconAnchor,
            middle.width, middle.height, middle.image, WriteMode::INVERT);
        display->addBitmapImage(c_ui_yIconAlign, iconAnchor - c_ui_iconSize - 3,
            top.width, top.height, top.image, WriteMode::INVERT);

        display->sendBuffer();
    }
}

void UserInterface::SetMenuContext(const std::vector<MenuEntry>& menu)
{
    currentMenu = menu;
}

void UserInterface::DrawMenu(uint index)
{
    if (display == nullptr) {
        return;
    }

    const uint8_t itemHeight = 10;
    const uint8_t itemPadding = 2;
    int8_t maxItems = displayHeight / itemHeight;
    const int8_t centerAnchorY = (displayHeight - itemHeight) >> 1;
    if (maxItems % 2 == 0) {
        maxItems--;
    }
    const int minIdx = index - (maxItems >> 1);
    const int maxIdx = index + (maxItems >> 1);

    display->clear();

    int8_t anchorOffset = -(maxItems >> 1) * itemHeight;
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

    const uint8_t iconAnchor = (displayHeight - c_ui_iconSize - 1) >> 1;
    display->addBitmapImage(c_ui_yIconAlign, iconAnchor,
        bmp_select.width, bmp_select.height, bmp_select.image);
    if (index > 0) {
        display->addBitmapImage(c_ui_yIconAlign, iconAnchor - c_ui_iconSize - 3,
            bmp_arrowUp.width, bmp_arrowUp.height, bmp_arrowUp.image);
    }
    if (index < currentMenu.size() - 1) {
        display->addBitmapImage(c_ui_yIconAlign, iconAnchor + c_ui_iconSize + 3,
            bmp_arrowDown.width, bmp_arrowDown.height, bmp_arrowDown.image);
    }

    display->sendBuffer();
}

void UserInterface::NewData(const QueueData* buffer)
{
    if (buffer == nullptr) {
        return;
    }

    const double tstamp = buffer->timestamp / 1000.0;

    // OLED data handler
    if (display != nullptr) {
        const uint8_t bottomOffsetSmall = displayHeight - 1 - 13;
        const uint8_t bottomOffsetLarge = bottomOffsetSmall - 4;
        const uint8_t centerOffsetSmall = bottomOffsetSmall - 16;
        const uint8_t centerOffsetLarge = centerOffsetSmall - 4;
        const uint8_t topOffsetSmall = centerOffsetSmall - 16;
        const uint8_t topOffsetLarge = topOffsetSmall - 4;

        bool refresh = true;
        switch (buffer->operation) {

        case QueueOperation::Greetings: {
            drawText(display, font_12x16, "PicoPOST", 1, 1);
            drawLine(display, 0, 18, 128, bottomOffsetSmall);
            drawText(display, font_8x8, "VCFMW", 127 - (strlen(PROJ_STR_VER) * 8), 22);
        } break;

        case QueueOperation::Volts: {
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

        case QueueOperation::P80Data:
        case QueueOperation::P80ResetActive:
        case QueueOperation::P80ResetCleared: {
            HistoryShift();
            fillRect(display, 0, 12, 127, displayHeight - 1, WriteMode::SUBTRACT);
            if (buffer->operation == QueueOperation::P80ResetActive) {
                sprintf(textBuffer[0], "R!");
            } else if (buffer->operation == QueueOperation::P80ResetCleared) {
                sprintf(textBuffer[0], "R_");
            } else {
                sprintf(textBuffer[0], "%02X", buffer->data);
            }
            const uint8_t itemSpace = 24;
            const uint8_t vertOffset = (displayHeight == 64) ? topOffsetSmall : bottomOffsetSmall;
            uint8_t horzOffset = 99;
            for (uint8_t idx = 0; idx < 5; idx++) {
                drawText(display, (idx == 0) ? font_12x16 : font_8x8,
                    textBuffer[idx], horzOffset, (idx == 0) ? vertOffset - 4 : vertOffset);
                horzOffset -= itemSpace;
            }
#if defined(PICOPOST_PRINT_CODE_DESCR)
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
#endif
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

    case QueueOperation::Greetings: {
        printf("-- PicoPOST@VCFMW --\n");
        printf("%s\n", creditsLine);
    } break;

    case QueueOperation::Volts: {
        printf("%10.3f | 5 V @ %.2f | 12 V @ %.2f | -12 V @ %.2f\n",
            tstamp, buffer->volts5, buffer->volts12, buffer->voltsN12);
    } break;

    case QueueOperation::P80Data: {
        printf("%10.3f | %02X @ %04Xh\n",
            tstamp, buffer->data, buffer->address);
    } break;

    case QueueOperation::P80ResetActive: {
        printf("Reset!\n");
    } break;

    case QueueOperation::P80ResetCleared: {
        printf("Reset cleared\n");
    } break;

    default: {
        // do nothing
    } break;
    }
}

void UserInterface::ClearBuffers()
{
    memset(textBuffer, '\0', sizeof(textBuffer));
}

MenuEntry UserInterface::GetMenuEntry(uint index)
{
    if (index < currentMenu.size()) {
        return currentMenu.at(index);
    } else {
        return { ProgramSelect::MainMenu, "" };
    }
}

void UserInterface::HistoryShift()
{
    for (uint i = c_maxHistory - 1; i > 0; i--) {
        memcpy(textBuffer[i], textBuffer[i - 1], c_maxStrlen);
    }
}

void UserInterface::UpdateSpritePosition(const Sprite& spr)
{
    if (spritePos.fullyHidden) {
        spritePos.offset = static_cast<uint8_t>(get_rand_32() % (displayWidth / 2));
        spritePos.y = -spr.height;
    } else {
        spritePos.y++;
    }

    // x = (y - c) / m
    spritePos.x = static_cast<int16_t>((spritePos.y - spritePos.offset) * spritePos.invSlope);

    spritePos.fullyHidden = (spritePos.x > displayWidth || spritePos.y > displayHeight);
    spritePos.fullyHidden |= (spritePos.x < -spr.width || spritePos.y < -spr.height);
}
