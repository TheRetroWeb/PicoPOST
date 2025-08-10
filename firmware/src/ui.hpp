/**
 * @file ui.hpp
 * @brief User interface is entirely here, visual output!
 *
 */

#ifndef PICOPOST_UI_HPP
#define PICOPOST_UI_HPP

#include "common.hpp"
#include "hardware/i2c.h"
#include "sh1106.hpp"
#include "ssd1306.hpp"
#include <utility>
#include <vector>

using OLEDLine = const char*;
using MenuEntry = std::pair<ProgramSelect, OLEDLine>;

class UserInterface {
public:
    UserInterface(pico_oled::OLED* display, pico_oled::Size dispSize);

    void ClearScreen();
    void SetScreenBrightness(uint8_t level);

    void DrawHeader(OLEDLine content);
    void DrawFooter(OLEDLine content);
    void DrawFullScreen(const Icon& bmp);
    void DrawScreenSaver(const Sprite& spr, uint8_t frameId);
    void DrawActions(const Icon& top, const Icon& middle, const Icon& bottom);

    void SetMenuContext(const std::vector<MenuEntry>& menu);
    void DrawMenu(uint index);

    void NewData(const QueueData* buffer, const size_t elements, const bool writeToOled = true);

    void ClearBuffers();

    MenuEntry GetMenuEntry(uint index);

    inline size_t GetMenuSize() const { return currentMenu.size(); }

private:
    struct SpritePosition {
        int16_t x;
        int16_t y;

        float invSlope;
        int16_t offset;
        bool fullyHidden;
    };

    static const size_t c_maxHistory { 10 };
    static const size_t c_maxStrlen { 15 };
    static const std::vector<MenuEntry> s_mainMenu;

    pico_oled::OLED* display { nullptr };
    pico_oled::Size dispSize { pico_oled::Size::W128xH32 };
    const uint8_t displayWidth { 128 };
    uint8_t displayHeight { 32 };
    std::vector<MenuEntry> currentMenu {};
    char textBuffer[c_maxHistory][c_maxStrlen] { '\0' };
    SpritePosition spritePos { 0 };
    uint16_t m_lastData { 0x0100 };

    void HistoryShift();
    void UpdateSpritePosition(const Sprite& spr);
};

#endif // PICOPOST_UI_HPP