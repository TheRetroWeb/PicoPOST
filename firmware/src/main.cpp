/**
 *    src/main.c
 *
 *    Do all the things!
 *
 **/

#include "main.hpp"

// Accessory libs
#include "gpioexp.hpp"
#include "sh1106.hpp"
#include "ssd1306.hpp"

// General utilites
#include "pins.h"
#include "proj.h"

// Primary functions
#include "logic.hpp"
#include "ui.hpp"

// System libs
#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include <stdio.h>
#include <string.h>

#define KEYIRQ_CLEANUP 200000 // 200ms cleanup routine
#define DEBOUNCE_RATE 3500 // 3.5ms debounce
#define I2C_CLK_RATE 400000 // Conservative 400 kHz I2C

enum UserMode {
    UM_I2CKeypad, // PCB rev6 + I2C remote
    UM_GPIOKeypad, // PCB rev5 + I2C/GPIO remote
    UM_Serial, // PCB rev? + no remote

    UM_Invalid, // Useless
};

queue_t dataQueue;

MCP23009* gpioexp = nullptr;
pico_oled::OLED* oled = nullptr;
UserInterface* ui = nullptr;

static volatile bool gotKeyIrq = false;
static volatile uint lastKey = 0;
static volatile uint lastKeyPressed = 0;
static volatile uint8_t keyPress = 0x00;
static volatile ProgramSelect logicSelect = PS_MAX_PROG;
static volatile UserMode hwUserMode = UM_I2CKeypad;

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
                ui->NewData(&buffer);
            }

            if (gotKeyIrq) {
                if (hwUserMode == UM_I2CKeypad) {
                    if (keyPress & KE_Back) {
                        Aux_ClearKeyIrq();
                        Logic_Stop();
                        logicSelect = PS_MAX_PROG;
                    }
                } else if (hwUserMode == UM_GPIOKeypad) {
                    if (keyPress == (KE_Up | KE_Down)) {
                        Aux_ClearKeyIrq();
                        Logic_Stop();
                        logicSelect = PS_MAX_PROG;
                    }
                }
            }
        }
    }
}

int64_t Aux_KeyDebounceCleanup(alarm_id_t id, void* userData)
{
    if (gotKeyIrq) {
        Aux_ClearKeyIrq();
    }
    return 0;
}

int64_t Aux_KeyDebounceRelease(alarm_id_t id, void* userData)
{
    if (hwUserMode == UM_I2CKeypad) {
        if (lastKeyPressed == 1) {
            auto maskRead = gpioexp->GetAll();
            if (lastKey & maskRead) {
                if (maskRead && (1 << GPIOEXP_KEY_UP)) {
                    keyPress |= KE_Up;
                }
                if (maskRead && (1 << GPIOEXP_KEY_DOWN)) {
                    keyPress |= KE_Down;
                }
                if (maskRead && (1 << GPIOEXP_KEY_SELECT)) {
                    keyPress |= KE_Select;
                }
                if (maskRead && (1 << GPIOEXP_KEY_BACK)) {
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
    add_alarm_in_us(KEYIRQ_CLEANUP, Aux_KeyDebounceCleanup, nullptr, true);
    lastKey = 0;
    lastKeyPressed = 0;
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

void Aux_SysInit()
{
    // Initialize data queue for async, multi-threaded data output
    queue_init(&dataQueue, sizeof(QueueData), MAX_QUEUE_LENGTH);

    // Onboard LED shows if we're ready for operation
    // Start off, turn back on when we're ready to enter main loop
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, false);

    // Initialize USB CDC serial port
    bool usb = stdio_usb_init();

    // Init I2C bus for remote
    i2c_init(i2c0, I2C_CLK_RATE);
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_I2C_SDA);
    gpio_pull_up(PIN_I2C_SCL);

    // Init and config GPIO expander
    printf("Looking for MCP23009... ");
    gpioexp = new MCP23009(i2c0, 0x40);
    if (gpioexp->IsConnected()) {
        hwUserMode = UM_I2CKeypad;
        gpioexp->Config(true, false, true, false);
        gpioexp->SetDirection(GPIOEXP_CFG_PINDIR);
        gpioexp->SetPolarity(GPIOEXP_CFG_PINPOL);
        gpioexp->SetInterruptSource(GPIOEXP_CFG_PINIRQ);
        gpioexp->SetReferenceState(0x00);
        gpioexp->SetInterruptEvent(GPIOEXP_CFG_PINIRQ);
        gpioexp->SetPullUps(GPIOEXP_CFG_PINPOL);

        printf("GPIO Exp OK! -> Assuming PCB rev6\n");

        // Initialize button IRQ and debounced key readout routine
        gpio_init(PIN_REMOTE_IRQ_R6);
        gpio_set_dir(PIN_REMOTE_IRQ_R6, GPIO_IN);
        gpio_pull_up(PIN_REMOTE_IRQ_R6);
        gpio_set_irq_enabled_with_callback(PIN_REMOTE_IRQ_R6, GPIO_IRQ_EDGE_FALL, true, Aux_KeyISR);
    } else {
        delete gpioexp;
        hwUserMode = UM_GPIOKeypad;

        printf("GPIO Exp KO! -> Assuming PCB rev5\n");

        // Initialize button GPIOs and key handler routine
        // Buttons are active low, so enable internal pull-ups
        uint pins[] = { PIN_KEY_UP_R5, PIN_KEY_DOWN_R5, PIN_KEY_SELECT_R5 };
        for (const uint pin : pins) {
            gpio_init(pin);
            gpio_set_dir(pin, GPIO_IN);
            gpio_pull_up(pin);
            gpio_set_irq_enabled_with_callback(pin, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, Aux_KeyISR);
        }
    }

    // Initialize OLED display on 1st I2C instance, @ 400 kHz, addr 0x3C
    bool dispType = false, dispSize = false, dispFlip = false;
    pico_oled::Size libOledSize = pico_oled::Size::W128xH32;
    if (hwUserMode == UM_I2CKeypad) {
        auto gpioConfig = gpioexp->GetAll();
        dispType = gpioConfig && (1 << GPIOEXP_IN_DISPTYPE);
        dispSize = gpioConfig && (1 << GPIOEXP_IN_DISPSIZE);
        dispFlip = gpioConfig && (1 << GPIOEXP_IN_DISPROT);
    }
    if (dispSize) {
        libOledSize = pico_oled::Size::W128xH64;
    }
    printf("Looking for OLED display... ");
    if (dispType) {
        oled = new pico_oled::SH1106(i2c0, 0x3C, libOledSize);
    } else {
        oled = new pico_oled::SSD1306(i2c0, 0x3C, libOledSize);
    }
    if (oled->IsConnected()) {
        if (dispType) {
            printf("OLED OK! -> Found SH1106\n");
        } else {
            printf("OLED OK! -> Found SSD1306\n");
        }
        oled->setOrientation(dispFlip);
    } else {
        delete oled;
        oled = nullptr;
        hwUserMode = UM_Serial;
        printf("OLED KO! -> Falling back to USB ACM\n");
    }

    ui = new UserInterface(oled, libOledSize);

    // Start async, multi-threaded data output dispatcher
    multicore_launch_core1(Aux_UI);
    ui->ClearScreen();
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    if (hwUserMode == UM_Serial && !stdio_usb_connected()) {
        int retry = 5;
        while (!stdio_usb_connected() && retry > 0) {
            stdio_usb_init();
            sleep_ms(100);
            retry--;
        }
        if (!stdio_usb_connected()) {
            hwUserMode = UM_Invalid;
        }
    }
}

int main()
{
    Aux_SysInit();

    // Primary operation loop
    /* What does it do?
       1. Do the up-down-enter thing with the menu
       2. Enter selected application
       3. App will exit when an appropriate exit condition is received
    */
    if (hwUserMode == UM_Invalid) {
        while (1) {
            gpio_put(PICO_DEFAULT_LED_PIN, true);
            sleep_ms(250);
            gpio_put(PICO_DEFAULT_LED_PIN, false);
            sleep_ms(250);
        }
    } else if (hwUserMode == UM_Serial) {
        Logic_Port80Reader(&dataQueue);
    } else {
        int menuIdx = 0;
        while (1) {
            logicSelect = PS_MAX_PROG;
            while (logicSelect == PS_MAX_PROG) {
                ui->DrawMenu(menuIdx);

                uint8_t keys = Aux_WaitForInput();
                switch (keys) {
                case KE_Down: {
                    if (menuIdx < ui->GetMenuSize() - 1)
                        menuIdx++;
                } break;

                case KE_Up: {
                    if (menuIdx > 0)
                        menuIdx--;
                } break;

                case KE_Select: {
                    logicSelect = ui->GetMenuEntry(menuIdx).first;
                } break;
                }
            }

            ui->ClearScreen();
            ui->DrawHeader(ui->GetMenuEntry(menuIdx).second);

            switch (logicSelect) {

            case PS_Port80Reader: {
                Logic_Port80Reader(&dataQueue);
            } break;

            case PS_Port84Reader: {
                Logic_Port80Reader(&dataQueue, 0x84);
            } break;

            case PS_Port90Reader: {
                Logic_Port80Reader(&dataQueue, 0x90);
            } break;
            
            case PS_Port300Reader: {
                Logic_Port80Reader(&dataQueue, 0x300);
            } break;
            
            case PS_Port378Reader: {
                Logic_Port80Reader(&dataQueue, 0x378);
            } break;

            case PS_VoltageMonitor: {
                Logic_VoltageMonitor(&dataQueue);
            } break;

            case PS_Info: {
                ui->DrawHeader("PicoPOST " PROJ_STR_VER);
                uint startIdx = 0;
                char creditsBlock[20] = { '\0' };
                const size_t lineLength = strlen(creditsLine);
                while (logicSelect == PS_Info) {
                    size_t blockLength = MIN(19, lineLength - startIdx);
                    memcpy(creditsBlock, creditsLine + startIdx, blockLength);
                    if (startIdx > (lineLength - 19)) {
                        memset(creditsBlock + blockLength, '\0', lineLength - startIdx);
                    }
                    ui->DrawFooter(creditsBlock);
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

            ui->ClearBuffers();
        }
    }

    return 0;
}
