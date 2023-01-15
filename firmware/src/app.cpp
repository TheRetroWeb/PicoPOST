#include "app.hpp"

// General utilites
#include "pins.h"
#include "proj.h"

// Primary functions
#include "logic.hpp"

// System libs
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>

Application* Application::instance { nullptr };

Application* Application::GetInstance()
{
    if (Application::instance == nullptr) {
        Application::instance = new Application();
    }
    return Application::instance;
}

int Application::PrimaryTask()
{
    while (true) {
        if (app_hwMode == UM_Invalid) {
            gpio_put(PICO_DEFAULT_LED_PIN, true);
            sleep_ms(250);
            gpio_put(PICO_DEFAULT_LED_PIN, false);
            sleep_ms(250);
        } else if (app_hwMode == UM_Serial) {
            Logic_Port80Reader(&app_dataQueue, false);
        } else {
            switch (app_select) {

                /*case PS_FullReader: {
                    ui->DrawFooter("Check serial output");
                    Logic_FullReader(&app_dataQueue, app_hwMode == UM_I2CKeypad);
                } break;*/

            case PS_Port80Reader: {
                Logic_Port80Reader(&app_dataQueue, app_hwMode == UM_I2CKeypad);
            } break;

            case PS_Port84Reader: {
                Logic_Port80Reader(&app_dataQueue, app_hwMode == UM_I2CKeypad, 0x84);
            } break;

            case PS_Port90Reader: {
                Logic_Port80Reader(&app_dataQueue, app_hwMode == UM_I2CKeypad, 0x90);
            } break;

            case PS_Port300Reader: {
                Logic_Port80Reader(&app_dataQueue, app_hwMode == UM_I2CKeypad, 0x300);
            } break;

            case PS_Port378Reader: {
                Logic_Port80Reader(&app_dataQueue, app_hwMode == UM_I2CKeypad, 0x378);
            } break;

            case PS_VoltageMonitor: {
                Logic_VoltageMonitor(&app_dataQueue, app_hwMode == UM_I2CKeypad);
            } break;

            case PS_Info: {
                app_ui->DrawHeader("PicoPOST " PROJ_STR_VER);
                uint startIdx = 0;
                char creditsBlock[20] = { '\0' };
                const size_t lineLength = strlen(creditsLine);
                while (app_select == PS_Info) {
                    size_t blockLength = MIN(19, lineLength - startIdx);
                    memcpy(creditsBlock, creditsLine + startIdx, blockLength);
                    if (startIdx > (lineLength - 19)) {
                        memset(creditsBlock + blockLength, '\0', lineLength - startIdx);
                    }
                    app_ui->DrawFooter(creditsBlock);
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
    }

    return 0;
}

void Application::UITask()
{
    Application* self = Application::GetInstance();

    // Register IRQ handlers and other UI-related functions to 2nd core
    switch (self->app_hwMode) {
    case UM_I2CKeypad: {
        // Initialize button IRQ and debounced key readout routine
        gpio_init(PIN_REMOTE_IRQ_R6);
        gpio_set_dir(PIN_REMOTE_IRQ_R6, GPIO_IN);
        gpio_pull_up(PIN_REMOTE_IRQ_R6);
        gpio_set_irq_enabled_with_callback(PIN_REMOTE_IRQ_R6, GPIO_IRQ_EDGE_FALL, true, self->I2CKeypadISR);
    } break;

    case UM_GPIOKeypad: {
        // Initialize button GPIOs and key handler routine
        // Buttons are active low, so enable internal pull-ups
        uint pins[] = { PIN_KEY_UP_R5, PIN_KEY_DOWN_R5, PIN_KEY_SELECT_R5 };
        for (const uint pin : pins) {
            gpio_init(pin);
            gpio_set_dir(pin, GPIO_IN);
            gpio_pull_up(pin);
            // gpio_set_irq_enabled_with_callback(pin, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, self->GPIOKeypadISR);
        }
    } break;
    }

    // Start UI loop
    int currMenuIdx = -1;
    while (true) {
        // Output data to display and serial
        if (self->app_select != PS_MAX_PROG) {
            uint count = queue_get_level(&self->app_dataQueue);
            if (count > 0) {
                QueueData buffer;
                queue_remove_blocking(&self->app_dataQueue, &buffer);
                self->app_ui->NewData(&buffer);
            }
        } else {
            if (self->app_menuIdx != currMenuIdx) {
                currMenuIdx = self->app_menuIdx;
                self->app_ui->DrawMenu(currMenuIdx);
            }
        }

        // Manage pending keypad operations
        if (self->key_irqFlag) {
            if (mutex_try_enter(&self->key_mutex, nullptr)) {
                if (self->app_select != PS_MAX_PROG) {
                    if ((self->app_hwMode == UM_I2CKeypad && (self->key_current & KE_Back))
                        || (self->app_hwMode == UM_GPIOKeypad && (self->key_current & (KE_Up | KE_Down)))) {
                        Logic_Stop();
                        self->app_select = PS_MAX_PROG;
                        self->app_ui->ClearBuffers();
                        currMenuIdx = -1;
                        while (!queue_is_empty(&self->app_dataQueue)) {
                            QueueData bogus;
                            queue_remove_blocking(&self->app_dataQueue, &bogus);
                        }
                    }
                } else {
                    switch (self->key_current) {
                    case KE_Down: {
                        if (self->app_menuIdx < self->app_ui->GetMenuSize() - 1)
                            self->app_menuIdx++;
                    } break;

                    case KE_Up: {
                        if (self->app_menuIdx > 0)
                            self->app_menuIdx--;
                    } break;

                    case KE_Select: {
                        self->app_ui->ClearScreen();
                        self->app_ui->DrawHeader(self->app_ui->GetMenuEntry(self->app_menuIdx).second);
                        self->app_select = self->app_ui->GetMenuEntry(self->app_menuIdx).first;
                    } break;
                    }
                }
                self->ClearKeypadIRQ();
                mutex_exit(&self->key_mutex);
            }
        } else {
            if (self->app_hwMode == UM_I2CKeypad && !gpio_get(PIN_REMOTE_IRQ_R6)) {
                self->hw_gpioexp->GetAll();
            }
        }
    }
}

int64_t Application::I2CKeypadDebouncer(alarm_id_t id, void* userData)
{
    Application* self = static_cast<Application*>(userData);
    mutex_enter_blocking(&self->key_mutex);
    if (self->key_debounceStage == 1) {
        auto maskRead = self->hw_gpioexp->GetAll();
        if (self->key_irqProbe & maskRead) {
            if (maskRead & (1 << GPIOEXP_KEY_UP)) {
                self->key_current |= KE_Up;
            }
            if (maskRead & (1 << GPIOEXP_KEY_DOWN)) {
                self->key_current |= KE_Down;
            }
            if (maskRead & (1 << GPIOEXP_KEY_SELECT)) {
                self->key_current |= KE_Select;
            }
            if (maskRead & (1 << GPIOEXP_KEY_BACK)) {
                self->key_current |= KE_Back;
            }
            self->key_irqFlag = true;
        }
    }
    self->key_irqProbe = 0;
    self->key_debounceStage = 0;
    mutex_exit(&self->key_mutex);
    return 0;
}

void Application::I2CKeypadISR(uint gpio, uint32_t event_mask)
{
    if (event_mask & GPIO_IRQ_EDGE_FALL) {
        Application* self = Application::GetInstance();
        if (self->key_irqProbe == 0) {
            switch (gpio) {
            case PIN_REMOTE_IRQ_R6: {
                self->key_debounceStage = 1;
                self->key_irqProbe = self->hw_gpioexp->GetInterruptFlag();
                add_alarm_in_us(DEBOUNCE_RATE, Application::I2CKeypadDebouncer, self, true);
            } break;
            }
        }
    }
}

void Application::ClearKeypadIRQ()
{
    this->key_irqFlag = false;
    this->key_current = KE_None;
    this->key_debounceStage = 0;
    this->key_irqProbe = 0;
}

Application::Application()
{
    // When starting from ISA bus, power might be unstable and I2C may be
    // unresponsive. Delay everything by some arbitrary amount of time
    sleep_ms(150);

    // Initialize keypad access mutex to avoid race conditions
    mutex_init(&this->key_mutex);

    // Initialize data queue for async, multi-threaded data output
    queue_init(&this->app_dataQueue, sizeof(QueueData), MAX_QUEUE_LENGTH);

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

    // Init and config GPIO expander
    printf("Looking for MCP23009... ");
    this->hw_gpioexp = new MCP23009(i2c0, 0x20);
    if (this->hw_gpioexp->IsConnected()) {
        printf("GPIO Exp OK! -> Assuming PCB rev6\n");
        this->app_hwMode = UM_I2CKeypad;
        this->hw_gpioexp->Config(true, false, false, false);
        this->hw_gpioexp->SetDirection(GPIOEXP_CFG_PINDIR);
        this->hw_gpioexp->SetPolarity(GPIOEXP_CFG_PINPOL);
        this->hw_gpioexp->SetInterruptSource(GPIOEXP_CFG_PINIRQ);
        this->hw_gpioexp->SetReferenceState(0x00);
        this->hw_gpioexp->SetInterruptEvent(GPIOEXP_CFG_PINIRQ);
        this->hw_gpioexp->SetPullUps(GPIOEXP_CFG_PINPOL);
    } else {
        printf("GPIO Exp KO! -> Assuming PCB rev5\n");
        delete this->hw_gpioexp;
        this->hw_gpioexp = nullptr;
        this->app_hwMode = UM_GPIOKeypad;
    }

    // Initialize OLED display on 1st I2C instance, @ 400 kHz, addr 0x3C
    bool dispType = false, dispSize = false, dispFlip = false;
    pico_oled::Size libOledSize = pico_oled::Size::W128xH32;
    if (this->app_hwMode == UM_I2CKeypad) {
        auto gpioConfig = this->hw_gpioexp->GetAll();
        dispType = gpioConfig & (1 << GPIOEXP_IN_DISPTYPE);
        dispSize = gpioConfig & (1 << GPIOEXP_IN_DISPSIZE);
        dispFlip = gpioConfig & (1 << GPIOEXP_IN_DISPROT);
    }
    if (dispSize) {
        libOledSize = pico_oled::Size::W128xH64;
    }
    printf("Looking for OLED display... ");
    if (dispType) {
        this->hw_oled = new pico_oled::SH1106(i2c0, 0x3C, libOledSize);
    } else {
        this->hw_oled = new pico_oled::SSD1306(i2c0, 0x3C, libOledSize);
    }
    if (this->hw_oled->IsConnected()) {
        if (dispType) {
            printf("OLED OK! -> Found SH1106\n");
        } else {
            printf("OLED OK! -> Found SSD1306\n");
        }
        this->hw_oled->setOrientation(dispFlip);
    } else {
        delete this->hw_oled;
        this->app_hwMode = UM_Serial;
        printf("OLED KO! -> Falling back to USB ACM\n");
    }

    this->app_ui = new UserInterface(this->hw_oled, libOledSize);
    this->app_ui->ClearScreen();

    if (this->app_hwMode == UM_Serial && !stdio_usb_connected()) {
        int retry = 5;
        while (!stdio_usb_connected() && retry > 0) {
            stdio_usb_init();
            sleep_ms(100);
            retry--;
        }
        if (!stdio_usb_connected()) {
            this->app_hwMode = UM_Invalid;
        }
    }

    gpio_put(PICO_DEFAULT_LED_PIN, true);
}
