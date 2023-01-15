/**
 *    src/main.c
 *
 *    Do all the things!
 *
 **/

// Primary functions
#include "app.hpp"

// System libs
#include "pico/multicore.h"
#include <string.h>

/*
int64_t Aux_KeyDebounceCleanup(alarm_id_t id, void* userData)
{
    if (gotKeyIrq) {
        Aux_ClearKeyIrq();
    }
    return 0;
}

int64_t Aux_KeyDebounceRelease(alarm_id_t id, void* userData)
{
    mutex_enter_blocking(&keypadAccess);
    whoLocked = 2;
    if (hwUserMode == UM_I2CKeypad) {
        if (lastKeyPressed == 1) {
            auto maskRead = gpioexp->GetAll();
            if (lastKey & maskRead) {
                if (maskRead & (1 << GPIOEXP_KEY_UP)) {
                    keyPress |= KE_Up;
                }
                if (maskRead & (1 << GPIOEXP_KEY_DOWN)) {
                    keyPress |= KE_Down;
                }
                if (maskRead & (1 << GPIOEXP_KEY_SELECT)) {
                    keyPress |= KE_Select;
                }
                if (maskRead & (1 << GPIOEXP_KEY_BACK)) {
                    keyPress |= KE_Back;
                }
                gotKeyIrq = true;
            }
        }
        gpio_set_irq_enabled(PIN_REMOTE_IRQ_R6, GPIO_IRQ_EDGE_FALL, true);
    } else if (hwUserMode == UM_GPIOKeypad) {
        if (lastKeyPressed == 1 && !gpio_get(lastKey)) {
            switch (lastKey) {
            case PIN_KEY_UP_R5: {
                keyPress |= KE_Up;
            } break;

            case PIN_KEY_DOWN_R5: {
                keyPress |= KE_Down;
            } break;

            case PIN_KEY_SELECT_R5: {
                keyPress |= KE_Select;
            } break;
            }
            gotKeyIrq = true;
        } else if (lastKeyPressed == 2 && gpio_get(lastKey)) {
            switch (lastKey) {
            case PIN_KEY_UP_R5: {
                keyPress &= ~KE_Up;
            } break;

            case PIN_KEY_DOWN_R5: {
                keyPress &= ~KE_Down;
            } break;

            case PIN_KEY_SELECT_R5: {
                keyPress &= ~KE_Select;
            } break;
            }
            gotKeyIrq = true;
        }
        gpio_set_irq_enabled(lastKey, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    }
    // add_alarm_in_us(KEYIRQ_CLEANUP, Aux_KeyDebounceCleanup, nullptr, true);
    lastKey = 0;
    lastKeyPressed = 0;
    whoLocked = 0;
    mutex_exit(&keypadAccess);
    return 0;
}

void Aux_KeyDebounceArm()
{
    if (hwUserMode == UM_I2CKeypad) {
        gpio_set_irq_enabled(PIN_REMOTE_IRQ_R6, GPIO_IRQ_EDGE_FALL, false);
    } else if (hwUserMode == UM_GPIOKeypad) {
        gpio_set_irq_enabled(lastKey, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, false);
    }
    add_alarm_in_us(DEBOUNCE_RATE, Aux_KeyDebounceRelease, nullptr, true);
}

void Aux_KeyISR(uint gpio, uint32_t event_mask)
{
    if (mutex_try_enter(&keypadAccess, nullptr)) {
        whoLocked = 3;
        if (lastKey == 0) {
            if (hwUserMode == UM_I2CKeypad) {
                if (event_mask & GPIO_IRQ_EDGE_FALL) {
                    switch (gpio) {
                    case PIN_REMOTE_IRQ_R6: {
                        lastKeyPressed = 1;
                        lastKey = gpioexp->GetInterruptFlag();
                        Aux_KeyDebounceArm();
                    } break;
                    }
                }
            } else if (hwUserMode == UM_GPIOKeypad) {
                if (event_mask & GPIO_IRQ_EDGE_FALL) {
                    switch (gpio) {
                    case PIN_KEY_SELECT_R5:
                    case PIN_KEY_UP_R5:
                    case PIN_KEY_DOWN_R5: {
                        lastKeyPressed = 1;
                        lastKey = gpio;
                        Aux_KeyDebounceArm();
                    } break;
                    }
                } else if (event_mask & GPIO_IRQ_EDGE_RISE) {
                    switch (gpio) {
                    case PIN_KEY_SELECT_R5:
                    case PIN_KEY_UP_R5:
                    case PIN_KEY_DOWN_R5: {
                        lastKeyPressed = 2;
                        lastKey = gpio;
                        Aux_KeyDebounceArm();
                    } break;
                    }
                }
            }
        }
        whoLocked = 0;
        mutex_exit(&keypadAccess);
    }
}
*/

int main()
{
    Application* app = Application::GetInstance();
    multicore_launch_core1(Application::UITask);
    return app->PrimaryTask();
}
