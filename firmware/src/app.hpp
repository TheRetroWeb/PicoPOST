#ifndef PICOPOST_SRC_APP
#define PICOPOST_SRC_APP

// System libs
#include "pico/sync.h"
#include "pico/time.h"
#include "pico/util/queue.h"
#include <atomic>
#include <memory>
#include <cstdint>
#include <string>

// Accessory libs
#include "gpioexp.hpp"

// Primary functions
#include "ui.hpp"
#include "logic.hpp"

// Serial is slow, but I hope we can make do without filling up 9.5 kBytes of
// bus data in less than we can empty the serial buffer at 115200 bps
#define MAX_QUEUE_LENGTH (1024)

// Conservative 400 kHz I2C
#define I2C_CLK_RATE (400000)

class Application {
public:
    enum class ErrorCodes : unsigned int {
        ERR_Fatal = 0,

        ERR_InvalidHWConfig = 1,
        ERR_MissingGPIOExpander = 2,
        ERR_MissingDisplay = 3
    };

    /**
     * @brief Returns the currently active application instance. If not yet
     * initialized, it calls the constructor.
     *
     * @return Pointer to the active instance
     */
    static Application* GetInstance();

    Application(); // DO NOT CALL EXPLICITLY PLZ

    /**
     * @brief Contains the application logic loop.
     */
    __attribute__((noreturn)) static void LogicTask();

    /**
     * @brief Contains the application UI loop, for keypad and display.
     *
     */
    __attribute__((noreturn)) static void UITask();

    /**
     * @brief Endlessly blinks for the specified about of times. 250ms between
     * each blink, 1250ms between two blink cycles.
     *
     * @param blinks The ErrorCodes enumeration doubles as an indicator for how
     * many blinks to show.
     */
    __attribute__((noreturn)) static void BlinkenHalt(ErrorCodes blinks);

    inline bool UseNewRemote() const
    {
        return (hwMode == UserMode::I2CKeypad);
    }

private:
    enum class DebouncerStep : uint8_t {
        Poll = 0,
        FirstTrigger,
        PendingEvent
    };

    enum class UserMode : uint8_t {
        I2CKeypad, // PCB rev6 + I2C remote
        GPIOKeypad, // PCB rev5 + I2C/GPIO remote
        Serial, // PCB rev? + no remote

        Invalid, // Useless
    };

    enum class TextScrollStep : uint8_t {
        DrawBitmap,
        BitmapOK,
        DrawHeader,
        DrawBlock,
        Wait,
        Quit
    };

    enum class StandbyStage : uint8_t {
        Active,
        Dimming,
        Standby,
        Screensaver
    };

    struct KeyboardState {
        uint8_t irqFlagPoll { 0x00 };
        uint8_t previousPress { 0x00 };
        uint64_t nextPoll { 0 };
        uint64_t debounceExpiry { 0 };
        DebouncerStep debounceStage { DebouncerStep::Poll };
        uint current { KE_None };
    };

    struct TextScroll {
        TextScrollStep stage { TextScrollStep::Quit };
        size_t sourceIdx { 0 };
        uint64_t tick { 0 };
        std::string output { "" };
    };

    // 20ms debounce, see https://www.eejournal.com/article/ultimate-guide-to-switch-debounce-part-4/
    static const uint64_t c_debounceRate { 20000 };
    static const size_t c_maxStrbuff { 14 };

    static const uint64_t c_standbyTimer { PICOPOST_STANDBY_TIMER * 1000000 };    
    static const uint8_t c_minBrightness { 0x09 };
    static const uint8_t c_maxBrightness { 0x7F };
    static const uint8_t c_brightnessStep { 1 };

    static std::unique_ptr<Application> instance;

    void PollI2CKeypad();
    void PollGPIOKeypad();
    void Keystroke();
    void UserOutput();
    void StandbyTick();

    std::unique_ptr<Logic> logic { nullptr };

    KeyboardState keyboard {};
    TextScroll textScroll {};
    
    UserMode hwMode { UserMode::Invalid };
    queue_t dataQueue;
    UserInterface* ui { nullptr };

    int app_currentMenuIdx { -1 };
    int app_newMenuIdx { 0 };
    ProgramSelect app_currentSelect { ProgramSelect::MainMenu };
    ProgramSelect app_newSelect { ProgramSelect::MainMenu };
    StandbyStage standby { StandbyStage::Active };
    uint8_t currBrightness { c_maxBrightness };
    uint64_t lastActivityTimer { 0 };
    uint64_t lastSsaverFrameChange { 0 };
    uint8_t lastSsaverFrame { 0 };

    MCP23009* hw_gpioexp { nullptr };
    pico_oled::OLED* hw_oled { nullptr };
};

#endif // PICOPOST_SRC_APP