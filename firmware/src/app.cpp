#include "app.hpp"

// General utilites
#include "pins.h"
#include "proj.h"

// Primary functions
#include "logic.hpp"

// System libs
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <cstdio>
#include <cstring>

std::unique_ptr<Application> Application::instance { nullptr };

Application* Application::GetInstance()
{
    if (Application::instance == nullptr) {
        Application::instance = std::make_unique<Application>();
    }
    return Application::instance.get();
}

__attribute__((noreturn)) void Application::PrimaryTask()
{
    while (true) {
        if (app_hwMode == UM_Invalid) {
            BlinkenHalt(ErrorCodes::ERR_InvalidHWConfig);
        } else if (app_hwMode == UM_Serial) {
            Logic::Port80Reader(&app_dataQueue, false);
        } else {
            switch (this->app_currentSelect) {

            /* TODO implement full stank reader
            case ProgramSelect::PS_FullReader: {
                ui->DrawFooter("Check serial output");
                Logic::FullReader(&app_dataQueue, app_hwMode == UM_I2CKeypad);
            } break;
            */

            case ProgramSelect::Port80Reader: {
                Logic::Port80Reader(&app_dataQueue, app_hwMode == UM_I2CKeypad);
            } break;

            case ProgramSelect::Port84Reader: {
                Logic::Port80Reader(&app_dataQueue, app_hwMode == UM_I2CKeypad, 0x84);
            } break;

            case ProgramSelect::Port90Reader: {
                Logic::Port80Reader(&app_dataQueue, app_hwMode == UM_I2CKeypad, 0x90);
            } break;

            case ProgramSelect::Port300Reader: {
                Logic::Port80Reader(&app_dataQueue, app_hwMode == UM_I2CKeypad, 0x300);
            } break;

            case ProgramSelect::Port378Reader: {
                Logic::Port80Reader(&app_dataQueue, app_hwMode == UM_I2CKeypad, 0x378);
            } break;

            case ProgramSelect::VoltageMonitor: {
                Logic::VoltageMonitor(&app_dataQueue, app_hwMode == UM_I2CKeypad);
            } break;

            default: {
                // come again?
            } break;
            }
        }
    }
}

__attribute__((noreturn)) void Application::UITask()
{
    auto self = Application::GetInstance();

    // Register IRQ handlers and other UI-related functions to 2nd core
    switch (self->app_hwMode) {
    case UM_I2CKeypad: {
        // Initialize button IRQ and debounced key readout routine
        gpio_init(PIN_REMOTE_IRQ_R6);
        gpio_set_dir(PIN_REMOTE_IRQ_R6, GPIO_IN);
        gpio_pull_up(PIN_REMOTE_IRQ_R6);
    } break;

    case UM_GPIOKeypad: {
        // Initialize button GPIOs and key handler routine
        // Buttons are active low, so enable internal pull-ups
        uint pins[] = { PIN_KEY_UP_R5, PIN_KEY_DOWN_R5, PIN_KEY_SELECT_R5 };
        for (const uint pin : pins) {
            gpio_init(pin);
            gpio_set_dir(pin, GPIO_IN);
            gpio_pull_up(pin);
        }
    } break;

    default: {
        // no keystrokes expected in this mode
    } break;
    }

    // Start UI loop
    while (true) {
        // Read keystrokes
        if (self->app_hwMode == UM_I2CKeypad) {
            self->PollI2CKeypad();
        } else if (self->app_hwMode == UM_GPIOKeypad) {
            // TODO self->PollGPIOKeypad();
        }

        // Handle pending keystrokes
        if (self->key_debounceStage == DebouncerStep::Debounce_PendingEvent) {
            self->Keystroke();
        }

        // Output data for user
        self->UserOutput();
    }
}

void Application::PollI2CKeypad()
{
    switch (this->key_debounceStage) {

    case DebouncerStep::Debounce_Poll: {
        if (time_us_64() >= this->key_nextPoll) {
            bool hwIrqPending = !gpio_get(PIN_REMOTE_IRQ_R6);
            if (hwIrqPending) {
                this->key_irqFlagPoll = this->hw_gpioexp->GetInterruptCapture();
                this->key_debounceExpiry = time_us_64() + this->c_debounceRate;
                this->key_debounceStage = DebouncerStep::Debounce_FirstTrigger;
            }
            this->key_nextPoll = time_us_64() + 50000;
        }
    } break;

    case DebouncerStep::Debounce_FirstTrigger: {
        if (time_us_64() >= this->key_debounceExpiry) {
            uint8_t currentKeymap = this->hw_gpioexp->GetAll(); // This clears the hw interrupt
            uint8_t persistentPress = (this->key_irqFlagPoll & currentKeymap);
            this->key_debounceStage = DebouncerStep::Debounce_Poll;
            if (persistentPress != this->key_previousPress) {
                if (persistentPress & (1 << GPIOEXP_KEY_UP)) {
                    this->key_current |= KE_Up;
                }
                if (persistentPress & (1 << GPIOEXP_KEY_DOWN)) {
                    this->key_current |= KE_Down;
                }
                if (persistentPress & (1 << GPIOEXP_KEY_SELECT)) {
                    this->key_current |= KE_Select;
                }
                if (persistentPress & (1 << GPIOEXP_KEY_BACK)) {
                    this->key_current |= KE_Back;
                }
                if (this->key_current != KE_None) {
                    this->key_debounceStage = DebouncerStep::Debounce_PendingEvent;
                }
            }
            this->key_previousPress = persistentPress;
            this->key_irqFlagPoll = 0x00;
            this->key_debounceExpiry = 0;
        }
    } break;

    default: {
        // Wait for an external event to reset the key poll state
    } break;
    }
}

void Application::Keystroke()
{
    /**
     * @brief DO NOT OUTPUT ANYTHING FROM HERE!
     * No OLED, no serial, no nothing.
     * Here, stored keystrokes should be used to reconfigure internal variables
     * so the next call to UserOutput() already knows what to do.
     * 
     */
    switch (this->app_currentSelect) {
    case ProgramSelect::MainMenu: {
        switch (this->key_current) {
        case KE_Down: {
            if (this->app_newMenuIdx < this->app_ui->GetMenuSize() - 1)
                this->app_newMenuIdx++;
        } break;

        case KE_Up: {
            if (this->app_newMenuIdx > 0)
                this->app_newMenuIdx--;
        } break;

        case KE_Select: {
            this->app_newSelect = this->app_ui->GetMenuEntry(this->app_currentMenuIdx).first;
            this->app_textScrollStage = TextScrollStep::DrawHeader;
        } break;

        default: {
            // other key strokes not expected
        } break;
        }
    } break;

    default: {
        if (this->key_current & KE_Back) {
            Logic::Stop();
            this->app_newSelect = ProgramSelect::MainMenu;
            while (!queue_is_empty(&this->app_dataQueue)) {
                QueueData bogus;
                queue_remove_blocking(&this->app_dataQueue, &bogus);
            }
            this->app_newMenuIdx = this->app_currentMenuIdx;
            this->app_currentMenuIdx = -1;
        }
    } break;
    }

    this->key_debounceStage = DebouncerStep::Debounce_Poll;
    this->key_current = KE_None;
}

void Application::UserOutput()
{
    if (this->app_newSelect != this->app_currentSelect) {
        this->app_ui->ClearBuffers();
        this->app_ui->ClearScreen();
        this->app_currentSelect = this->app_newSelect;
        this->app_ui->DrawHeader(this->app_ui->GetMenuEntry(this->app_currentMenuIdx).second);
    }

    switch (this->app_currentSelect) {
    case ProgramSelect::MainMenu: {
        if (this->app_newMenuIdx != this->app_currentMenuIdx) {
            this->app_currentMenuIdx = this->app_newMenuIdx;
            this->app_ui->DrawMenu(this->app_currentMenuIdx);
        }
    } break;

    case ProgramSelect::Info: {
        switch (this->app_textScrollStage) {
            case TextScrollStep::DrawHeader: {
                this->app_ui->DrawHeader("PicoPOST " PROJ_STR_VER);
                this->app_textScrollSourceIdx = 0;
                this->app_textScrollStage = TextScrollStep::DrawBlock;
            } break;

            case TextScrollStep::DrawBlock: {
                this->app_textScrollOutput.clear();
                this->app_textScrollOutput.assign(creditsLine + this->app_textScrollSourceIdx);
                this->app_textScrollOutput = this->app_textScrollOutput.substr(0, c_maxStrbuff);
                this->app_ui->DrawFooter(this->app_textScrollOutput.c_str());
                this->app_textScrollSourceIdx++;
                this->app_textScrollSourceIdx %= strlen(creditsLine);
                this->app_textScrollTick = time_us_64() + 250000;
                if (this->app_textScrollSourceIdx == 0)
                    this->app_textScrollTick = time_us_64() + 1000000;
                this->app_textScrollStage = TextScrollStep::Wait;
            } break;

            case TextScrollStep::Wait: {
                if (time_us_64() >= this->app_textScrollTick)
                    this->app_textScrollStage = TextScrollStep::DrawBlock;
            } break;

            case TextScrollStep::Quit: {
                this->app_textScrollSourceIdx = 0;
                this->app_textScrollTick = 0;
            } break;
        }
    } break;

    default: {
        uint count = queue_get_level(&this->app_dataQueue);
        if (count > 0) {
            QueueData buffer;
            queue_remove_blocking(&this->app_dataQueue, &buffer);
            this->app_ui->NewData(&buffer);
        }
    } break;
    }
}

__attribute__((noreturn)) void Application::BlinkenHalt(ErrorCodes blinks)
{
    while (true) {
        for (unsigned int i = 0; i < static_cast<unsigned int>(blinks); i++) {
            gpio_put(PICO_DEFAULT_LED_PIN, true);
            sleep_ms(250);
            gpio_put(PICO_DEFAULT_LED_PIN, false);
            sleep_ms(250);
        }
        sleep_ms(1250);
    }
}

Application::Application()
{
    // When starting from ISA bus, power might be unstable and I2C may be
    // unresponsive. Delay everything by some arbitrary amount of time
    sleep_ms(150);

    // Initialize data queue for async, multi-threaded data output
    queue_init(&this->app_dataQueue, sizeof(QueueData), MAX_QUEUE_LENGTH);

    // Onboard LED shows if we're ready for operation
    // Start off, turn back on when we're ready to enter main loop
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, false);

    // Initialize USB CDC serial port
    // bool usb = stdio_usb_init();

    // Init I2C bus for remote
    i2c_init(i2c0, I2C_CLK_RATE);
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);

    // Init and config GPIO expander
    printf("Looking for MCP23009... ");
    this->hw_gpioexp = new MCP23009(i2c0, 0x20);
    if (this->hw_gpioexp->IsConnected()) {
        printf("GPIO Exp OK! -> Assuming PCB rev6+\n");
        this->app_hwMode = UM_I2CKeypad;
        this->hw_gpioexp->Config(true, false, false, false);
        this->hw_gpioexp->SetDirection(GPIOEXP_CFG_PINDIR);
        this->hw_gpioexp->SetPolarity(GPIOEXP_CFG_PINPOL);
        this->hw_gpioexp->SetInterruptSource(GPIOEXP_CFG_PINIRQ);
        this->hw_gpioexp->SetReferenceState(0x00);
        this->hw_gpioexp->SetInterruptEvent(GPIOEXP_CFG_PINIRQ);
        this->hw_gpioexp->SetPullUps(GPIOEXP_CFG_PINPOL);
    } else {
        /*printf("GPIO Exp KO! -> Assuming PCB rev5\n");
        delete this->hw_gpioexp;
        this->hw_gpioexp = nullptr;
        this->app_hwMode = UM_GPIOKeypad;*/
        BlinkenHalt(ErrorCodes::ERR_MissingGPIOExpander);
    }

    // Initialize OLED display on 1st I2C instance, @ 400 kHz, addr 0x3C
    bool dispType = false;
    bool dispSize = false;
    bool dispFlip = false;
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
        /*delete this->hw_oled;
        this->app_hwMode = UM_Serial;
        printf("OLED KO! -> Falling back to USB ACM\n");*/
        BlinkenHalt(ErrorCodes::ERR_MissingDisplay);
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
