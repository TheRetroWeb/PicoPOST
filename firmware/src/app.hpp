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

// Serial is slow, but I hope we can make do without filling up 9.5 kBytes of
// bus data in less than we can empty the serial buffer at 115200 bps
#define MAX_QUEUE_LENGTH (1024)

// Conservative 400 kHz I2C
#define I2C_CLK_RATE (400000)

#define IRQ_RESET_VALUE (0x00U)

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
     * @brief Contains the primary application logic loop.
     *
     * @return Just an integer as valid return for the main entrypoint.
     */
    __attribute__((noreturn)) void PrimaryTask();

    /**
     * @brief Contains the secondary application UI loop. Must be run on the 2nd
     * RP2040 core. Being static, it recovers "itself" using the static instance.
     *
     */
    __attribute__((noreturn)) static void UITask();

    /**
     * @brief Endlessly blinks for the specified about of times. 250ms between
     * each blink, 1250ms between two blink cycles.
     *
     * @param blinks The ErrorCodes enumerations doubles as an indicator for how
     * many blinks to show.
     */
    __attribute__((noreturn)) static void BlinkenHalt(ErrorCodes blinks);

private:
    enum DebouncerStep {
        Debounce_Poll = 0,
        Debounce_FirstTrigger,
        Debounce_PendingEvent
    };

    enum UserMode {
        UM_I2CKeypad, // PCB rev6 + I2C remote
        UM_GPIOKeypad, // PCB rev5 + I2C/GPIO remote
        UM_Serial, // PCB rev? + no remote

        UM_Invalid, // Useless
    };

    enum class TextScrollStep : uint8_t {
        DrawHeader,
        DrawBlock,
        Wait,
        Quit
    };

    // 20ms debounce, see https://www.eejournal.com/article/ultimate-guide-to-switch-debounce-part-4/
    const uint64_t c_debounceRate { 20000ULL };
    const size_t c_maxStrbuff { 20 };

    static std::unique_ptr<Application> instance;

    void PollI2CKeypad();
    void PollGPIOKeypad();
    void Keystroke();
    void UserOutput();

    uint8_t key_irqFlagPoll { 0x00 };
    uint8_t key_previousPress { 0x00 };
    uint64_t key_nextPoll { 0 };
    uint64_t key_debounceExpiry { 0 };
    DebouncerStep key_debounceStage { DebouncerStep::Debounce_Poll };
    uint key_current { KE_None };
    
    UserMode app_hwMode { UM_Invalid };
    queue_t app_dataQueue;
    UserInterface* app_ui { nullptr };
    int app_currentMenuIdx { -1 };
    int app_newMenuIdx { 0 };
    ProgramSelect app_currentSelect { ProgramSelect::MainMenu };
    ProgramSelect app_newSelect { ProgramSelect::MainMenu };

    TextScrollStep app_textScrollStage { TextScrollStep::Quit };
    size_t app_textScrollSourceIdx { 0 };
    uint64_t app_textScrollTick { 0 };
    std::string app_textScrollOutput { "" };

    MCP23009* hw_gpioexp { nullptr };
    pico_oled::OLED* hw_oled { nullptr };
};

#endif // PICOPOST_SRC_APP