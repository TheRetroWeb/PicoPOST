/**
 *    src/main.c
 *
 *    Do all the things!
 *
 **/

#include "main.hpp"
#include "logic.hpp"
#include "pins.h"
#include "proj.h"
#include "ui.hpp"

#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include <string.h>
#include <stdio.h>

#define DEBOUNCE_RATE 3500

queue_t dataQueue;

static volatile bool gotKeyIrq = false;
static volatile uint lastKey = 0;
static volatile uint lastKeyPressed = 0;
static volatile uint8_t keyPress = 0x00;
static volatile ProgramSelect logicSelect = PS_MAX_PROG;

void Aux_ClearKeyIrq()
{
    gotKeyIrq = false;
    keyPress = 0x00;
}

void Aux_UI()
{
    while (1) {
        if (logicSelect != PS_MAX_PROG) {
            uint count = queue_get_level(&dataQueue);
            if (count > 0) {
                QueueData buffer;
                queue_remove_blocking(&dataQueue, &buffer);

                UI_PrintSerial(&buffer);
                UI_DataOLED(&buffer);
            }

            if (gotKeyIrq) {
                if (keyPress == (KE_Up | KE_Down)) {
                    Aux_ClearKeyIrq();
                    Logic_Stop();
                    logicSelect = PS_MAX_PROG;
                }
            }
        }
    }
}

int64_t Aux_KeyDebounceRelease(alarm_id_t id, void* userData)
{
    if (lastKeyPressed == 1 && !gpio_get(lastKey)) {
        switch (lastKey) {
        case PIN_KEY_UP: {
            keyPress |= KE_Up;
        } break;

        case PIN_KEY_DOWN: {
            keyPress |= KE_Down;
        } break;

        case PIN_KEY_SELECT: {
            keyPress |= KE_Select;
        } break;
        }
        gotKeyIrq = true;
    } else if (lastKeyPressed == 2 && gpio_get(lastKey)) {
        switch (lastKey) {
        case PIN_KEY_UP: {
            keyPress &= ~KE_Up;
        } break;

        case PIN_KEY_DOWN: {
            keyPress &= ~KE_Down;
        } break;

        case PIN_KEY_SELECT: {
            keyPress &= ~KE_Select;
        } break;
        }
        gotKeyIrq = true;
    }
    gpio_set_irq_enabled(lastKey, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    lastKey = 0;
    lastKeyPressed = 0;
    return 0;
}

void Aux_KeyDebounceArm(uint pin)
{
    lastKey = pin;
    gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, false);
    add_alarm_in_us(DEBOUNCE_RATE, Aux_KeyDebounceRelease, nullptr, true);
}

void Aux_KeyISR(uint gpio, uint32_t event_mask)
{
    if (lastKey == 0) {
        if (event_mask & GPIO_IRQ_EDGE_FALL) {
            switch (gpio) {
            case PIN_KEY_SELECT:
            case PIN_KEY_UP:
            case PIN_KEY_DOWN: {
                lastKeyPressed = 1;
                Aux_KeyDebounceArm(gpio);
            } break;
            }
        } else if (event_mask & GPIO_IRQ_EDGE_RISE) {
            switch (gpio) {
            case PIN_KEY_SELECT:
            case PIN_KEY_UP:
            case PIN_KEY_DOWN: {
                lastKeyPressed = 2;
                Aux_KeyDebounceArm(gpio);
            } break;
            }
        }
    }
    
}

uint8_t Aux_WaitForInput()
{
    gotKeyIrq = false;
    while (!gotKeyIrq)
        ;
    uint8_t keys = keyPress;
    Aux_ClearKeyIrq();
    return keys;
}

int main()
{
    // Initialize USB CDC serial port
    stdio_init_all();

    // Onboard LED shows if we're ready for operation
    // Start off, turn back on when we're ready to enter main loop
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, false);

    // Initialize button GPIOs and key handler routine
    // Buttons are active low, so enable internal pull-ups
    uint pins[] = { PIN_KEY_UP, PIN_KEY_DOWN, PIN_KEY_SELECT };
    for (const uint pin : pins) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
        gpio_set_irq_enabled_with_callback(pin,
            GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, Aux_KeyISR);
    }

    // Initialize data queue for async, multi-threaded data output
    queue_init(&dataQueue, sizeof(QueueData), MAX_QUEUE_LENGTH);

    // Initialize OLED display on 1st I2C instance, @ 400 kHz, addr 0x3C
    // TODO: read from GPIO expander bits [5..7] for display config
    UI_InitOLED(i2c0, 400000, 0x3C, 0, 0);

    // Start async, multi-threaded data output dispatcher
    multicore_launch_core1(Aux_UI);

    // Greetings!
    QueueData qd {
        QO_Greetings
    };
    UI_PrintSerial(&qd);
    UI_DataOLED(&qd);

    // Ready!
    sleep_ms(2000);
    UI_ClearScreenOLED();
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    // Primary operation loop
    /* What does it do?
       1. Do the up-down-enter thing with the menu
       2. Enter selected application
       3. App will exit when an appropriate exit condition is received
    */
    int menuIdx = 0;
    while (1) {
        logicSelect = PS_MAX_PROG;
        while (logicSelect == PS_MAX_PROG) {
            UI_DrawMenuOLED(menuIdx);

            uint8_t keys = Aux_WaitForInput();
            switch (keys) {
            case KE_Down: {
                if (menuIdx < PS_MAX_PROG - 1)
                    menuIdx++;
            } break;

            case KE_Up: {
                if (menuIdx > 0)
                    menuIdx--;
            } break;

            case KE_Select: {
                // Direct cast from int to ProgramSelect. EWW!
                logicSelect = (ProgramSelect)menuIdx;
            } break;
            }
        }

        UI_ClearScreenOLED();

        switch (logicSelect) {

        case PS_Port80Reader: {
            UI_SetHeaderOLED("Port 80h");
            Logic_Port80Reader(&dataQueue);
            UI_ClearBuffers();
        } break;

        case PS_VoltageMonitor: {
            UI_SetHeaderOLED("Voltage monitor");
            Logic_VoltageMonitor(&dataQueue);
            UI_ClearBuffers();
        } break;

        case PS_Info: {
            UI_SetHeaderOLED("PicoPOST " PROJ_STR_VER);
            uint startIdx = 0;
            char creditsBlock[20] = { '\0' };
            const size_t lineLength = strlen(creditsLine);
            while (logicSelect == PS_Info) {
                size_t blockLength = MIN(19, lineLength - startIdx);
                memcpy(creditsBlock, creditsLine + startIdx, blockLength);
                if (startIdx > (lineLength - 19)) {
                    memset(creditsBlock + blockLength, '\0', lineLength - startIdx);
                }
                UI_SetTextOLED(creditsBlock);
                startIdx++;
                if (startIdx == lineLength) {
                    startIdx = 0;
                    sleep_ms(1000);
                } else {
                    sleep_ms(300);
                }
            }
        } break;

        default: {
            // come again?
        } break;
        }
    }

    return 0;
}