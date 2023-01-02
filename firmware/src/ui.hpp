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

#define MAX_HISTORY 5
#define MAX_VALUE_LEN 15

using OLEDLine = const char*;
using MenuEntry = std::pair<ProgramSelect, OLEDLine>;

class UserInterface {
public:
    UserInterface(pico_oled::OLED* display, pico_oled::Size dispSize);

    void ClearScreen();

    void DrawHeader(OLEDLine content);
    void DrawFooter(OLEDLine content);

    void SetMenuContext(const std::vector<MenuEntry>& menu);
    void DrawMenu(uint index);

    void NewData(const QueueData* buffer);

    void ClearBuffers();

private:
    pico_oled::OLED* display { nullptr };
    pico_oled::Size dispSize { pico_oled::Size::W128xH32 };
    std::vector<MenuEntry> currentMenu {};
    char textBuffer[MAX_HISTORY][MAX_VALUE_LEN] { '\0' };

    static const std::vector<MenuEntry> s_mainMenu;

    void HistoryShift();
};

#endif // PICOPOST_UI_HPP