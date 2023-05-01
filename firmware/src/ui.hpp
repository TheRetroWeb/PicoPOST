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

    void DrawHeader(OLEDLine content);
    void DrawFooter(OLEDLine content);
    void DrawFullScreen(const Bitmap& bmp);
    void DrawActions(const Bitmap& top, const Bitmap& middle, const Bitmap& bottom);

    void SetMenuContext(const std::vector<MenuEntry>& menu);
    void DrawMenu(uint index);

    void NewData(const QueueData* buffer);

    void ClearBuffers();

    MenuEntry GetMenuEntry(uint index);

    inline size_t GetMenuSize() const { return currentMenu.size(); }

private:
    static const size_t c_maxHistory { 10 };
    static const size_t c_maxStrlen { 15 };

    pico_oled::OLED* display { nullptr };
    pico_oled::Size dispSize { pico_oled::Size::W128xH32 };
    uint8_t displayHeight { 32 };
    std::vector<MenuEntry> currentMenu {};
    char textBuffer[c_maxHistory][c_maxStrlen] { '\0' };

    static const std::vector<MenuEntry> s_mainMenu;

    void HistoryShift();
};

#endif // PICOPOST_UI_HPP