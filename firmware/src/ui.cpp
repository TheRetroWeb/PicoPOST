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
    { ProgramSelect::BusDump, "Bus dump" },
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
    if (display == nullptr)
        return;
    if (frameId > spr.frameCount)
        return;

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

void UserInterface::NewData(const QueueData* buffer, const size_t elements)
{
    if (buffer == nullptr) {
        return;
    }

    const uint8_t bottomOffsetSmall = displayHeight - 1 - 13;
    const uint8_t bottomOffsetLarge = bottomOffsetSmall - 4;
    const uint8_t centerOffsetSmall = bottomOffsetSmall - 16;
    const uint8_t centerOffsetLarge = centerOffsetSmall - 4;
    const uint8_t topOffsetSmall = centerOffsetSmall - 16;
    const uint8_t topOffsetLarge = topOffsetSmall - 4;

    // OLED data handler
    enum class OLEDRefreshOperation {
        None,
        Greet,
        Volts,
        Bus,
    };

    OLEDRefreshOperation oledRefresh { OLEDRefreshOperation::None };
    for (uint idx = 0; idx < elements; idx++) {
        const auto currItem = &buffer[idx];
        switch (currItem->operation) {

        case QueueOperation::Greetings: {
            oledRefresh = OLEDRefreshOperation::Greet;
        } break;

        case QueueOperation::Volts: {
            sprintf(textBuffer[0], "%01.2f", currItem->volts5);
            sprintf(textBuffer[1], "%02.2f", currItem->volts12);
            oledRefresh = OLEDRefreshOperation::Volts;
        } break;

        case QueueOperation::P80Data: {
            if (currItem->data != m_lastData) {
                HistoryShift();
                sprintf(textBuffer[0], "%02X", currItem->data);
                m_lastData = currItem->data;
                oledRefresh = OLEDRefreshOperation::Bus;
            }
        } break;

        case QueueOperation::P80ResetActive:
        case QueueOperation::P80ResetCleared: {
            HistoryShift();
            if (currItem->operation == QueueOperation::P80ResetActive) {
                sprintf(textBuffer[0], "R!");
            } else {
                sprintf(textBuffer[0], "R_");
            }
            m_lastData = 0x0100;
        } break;

        default: {
            // do nothing
        } break;
        }

        // USB ACM serial output
        const double tstamp = currItem->timestamp / 1000.0;
        switch (currItem->operation) {
        case QueueOperation::Greetings: {
            printf("-- PicoPOST " PROJ_STR_VER " --\n");
            printf("%s\n", creditsLine);
        } break;

        case QueueOperation::Volts: {
            printf("%10.3f | 5 V @ %s | 12 V @ %s\n", tstamp, textBuffer[0], textBuffer[1]);
        } break;

        case QueueOperation::P80Data: {
            printf("%10.3f | %02X @ %04Xh\n", tstamp, currItem->data, currItem->address);
        } break;

        case QueueOperation::P80ResetActive: {
            printf("Reset asserted!\n");
        } break;

        case QueueOperation::P80ResetCleared: {
            printf("Reset cleared\n");
        } break;

        default: {
            // do nothing
        } break;
        }
    }

    if (display != nullptr && oledRefresh != OLEDRefreshOperation::None) {
        switch (oledRefresh) {
        case OLEDRefreshOperation::Bus: {
            fillRect(display, 0, 12, 127, displayHeight - 1, WriteMode::SUBTRACT);
            const uint8_t itemSpace = 24;
            const uint8_t vertOffset = (displayHeight == 64) ? topOffsetSmall : bottomOffsetSmall;
            uint8_t horzOffset = 99;
            for (uint8_t idx = 0; idx < 5; idx++) {
                drawText(display, (idx == 0) ? font_12x16 : font_8x8,
                    textBuffer[idx], horzOffset, (idx == 0) ? vertOffset - 4 : vertOffset);
                horzOffset -= itemSpace;
            }
        } break;

        case OLEDRefreshOperation::Volts: {
            fillRect(display, 0, 9, 127, displayHeight - 1, WriteMode::SUBTRACT);
            if (displayHeight == 32) {
                drawText(display, font_5x8, "+5V", 2, 9);
                drawText(display, font_8x8, textBuffer[0], 2, bottomOffsetSmall);

                drawText(display, font_5x8, "+12V", 60, 9);
                drawText(display, font_8x8, textBuffer[1], 60, bottomOffsetSmall);
            } else if (displayHeight == 64) {
                drawText(display, font_5x8, "+5V", 4, 11);
                drawText(display, font_8x8, textBuffer[0], 4, 23);

                drawText(display, font_5x8, "+12V", 67, 11);
                drawText(display, font_8x8, textBuffer[1], 67, 23);
            }
        } break;

        case OLEDRefreshOperation::Greet: {
            drawText(display, font_12x16, "PicoPOST", 1, 1);
            drawLine(display, 0, 18, 128, bottomOffsetSmall);
            drawText(display, font_8x8, PROJ_STR_VER, 127 - (strlen(PROJ_STR_VER) * 8), 22);
        } break;
        }

        display->sendBuffer();
    }
}

void UserInterface::ClearBuffers()
{
    m_lastData = 0x0100;
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
