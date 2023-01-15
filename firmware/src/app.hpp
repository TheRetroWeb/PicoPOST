#ifndef PICOPOST_SRC_APP
#define PICOPOST_SRC_APP

// System libs
#include "pico/sync.h"
#include "pico/time.h"
#include "pico/util/queue.h"
#include <atomic>
#include <stdint.h>

// Accessory libs
#include "gpioexp.hpp"

// Primary functions
#include "ui.hpp"

enum UserMode {
    UM_I2CKeypad, // PCB rev6 + I2C remote
    UM_GPIOKeypad, // PCB rev5 + I2C/GPIO remote
    UM_Serial, // PCB rev? + no remote

    UM_Invalid, // Useless
};

// Serial is slow, but I hope we can make do without filling up 9.5 kBytes of
// bus data in less than we can empty the serial buffer at 115200 bps
#define MAX_QUEUE_LENGTH 1024

// 3.5ms debounce
#define DEBOUNCE_RATE 3500

// Conservative 400 kHz I2C
#define I2C_CLK_RATE 1000000

class Application {
public:
    /**
     * @brief Returns the currently active application instance. If not yet
     * initialized, it calls the constructor.
     *
     * @return Pointer to the active instance
     */
    static Application* GetInstance();

    /**
     * @brief Contains the primary application logic loop.
     *
     * @return Just an integer as valid return for the main entrypoint.
     */
    int PrimaryTask();

    /**
     * @brief Contains the secondary application UI loop. Must be run on the 2nd
     * RP2040 core. Being static, it recovers "itself" using the static instance.
     *
     */
    static void UITask();

private:
    static Application* instance;

    Application();

    /**
     * @brief Interrupt handler for the I2C keypad.
     */
    static void I2CKeypadISR(uint gpio, uint32_t event_mask);

    /**
     * @brief Interrupt handler for the GPIO keypad.
     * Will probably be deleted soon.
     */
    static void GPIOKeypadISR(uint gpio, uint32_t event_mask);

    /**
     * @brief Asynchronous key debouncer handler for the I2C keypad.
     */
    static int64_t I2CKeypadDebouncer(alarm_id_t id, void* userData);

    /**
     * @brief Asynchronous key debouncer handler for the GPIO keypad.
     * Will probably be deleted soon.
     */
    static int64_t GPIOKeypadDebouncer(alarm_id_t id, void* userData);

    void ClearKeypadIRQ();

    mutex_t key_mutex;
    std::atomic<bool> key_irqFlag { false };
    uint key_irqProbe { 0 };
    uint key_debounceStage { 0 };
    uint key_current { KE_None };

    ProgramSelect app_select { PS_MAX_PROG };
    UserMode app_hwMode { UM_Invalid };
    queue_t app_dataQueue;
    UserInterface* app_ui { nullptr };
    int app_menuIdx { 0 };

    MCP23009* hw_gpioexp { nullptr };
    pico_oled::OLED* hw_oled { nullptr };
};

#endif // PICOPOST_SRC_APP