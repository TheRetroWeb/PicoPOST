#include "app.hpp"

// General utilites
#include "bitmaps.hpp"
#include "pins.h"
#include "proj.h"

// Primary functions
#include "logic.hpp"

// System libs
#include "hardware/gpio.h"
#include "pico/bootrom.h"
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

__attribute__((noreturn)) void Application::LogicTask()
{
    auto self = Application::GetInstance();

    while (true) {
        self->logic->Prepare();
        if (self->hwMode == UserMode::Serial) {
            self->logic->AddressReader(&self->dataQueue, false);
        } else {
            switch (self->app_currentSelect) {

                /* TODO implement full stank reader
                case ProgramSelect::PS_FullReader: {
                    self->ui->DrawFooter("Check serial output");
                    self->logic->FullReader(&self->dataQueue, self->UseNewRemote());
                } break;
                */

            case ProgramSelect::Port80Reader: {
                self->logic->AddressReader(&self->dataQueue, self->UseNewRemote());
            } break;

            case ProgramSelect::Port84Reader: {
                self->logic->AddressReader(&self->dataQueue, self->UseNewRemote(), 0x84);
            } break;

            case ProgramSelect::Port90Reader: {
                self->logic->AddressReader(&self->dataQueue, self->UseNewRemote(), 0x90);
            } break;

            case ProgramSelect::Port300Reader: {
                self->logic->AddressReader(&self->dataQueue, self->UseNewRemote(), 0x300);
            } break;

            case ProgramSelect::Port378Reader: {
                self->logic->AddressReader(&self->dataQueue, self->UseNewRemote(), 0x378);
            } break;

            case ProgramSelect::VoltageMonitor: {
                self->logic->VoltageMonitor(&self->dataQueue, self->UseNewRemote());
            } break;

            default: {
                // do nothing
            } break;
            }
        }
        sleep_ms(150);
    }
}

__attribute__((noreturn)) void Application::UITask()
{
    auto self = Application::GetInstance();

    // Set up GPIO for appropriate keypress handling
    switch (self->hwMode) {
    case UserMode::I2CKeypad: {
        // Initialize button IRQ and debounced key readout routine
        gpio_init(PIN_REMOTE_IRQ_R6);
        gpio_set_dir(PIN_REMOTE_IRQ_R6, GPIO_IN);
        gpio_pull_up(PIN_REMOTE_IRQ_R6);
    } break;

    case UserMode::GPIOKeypad: {
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
        if (self->UseNewRemote()) {
            self->PollI2CKeypad();
        } else if (self->hwMode == UserMode::GPIOKeypad) {
            // TODO self->PollGPIOKeypad();
        }

        // Handle pending keystrokes
        if (self->keyboard.debounceStage == DebouncerStep::PendingEvent) {
            self->lastActivityTimer = time_us_64();
            self->Keystroke();
        }

        // Output data for user
        self->UserOutput();

        self->StandbyTick();
    }
}

void Application::PollI2CKeypad()
{
    switch (this->keyboard.debounceStage) {

    case DebouncerStep::Poll: {
        if (time_us_64() >= this->keyboard.nextPoll) {
            bool hwIrqPending = !gpio_get(PIN_REMOTE_IRQ_R6);
            if (hwIrqPending) {
                this->keyboard.irqFlagPoll = this->hw_gpioexp->GetInterruptCapture();
                this->keyboard.debounceExpiry = time_us_64() + Application::c_debounceRate;
                this->keyboard.debounceStage = DebouncerStep::FirstTrigger;
            }
            this->keyboard.nextPoll = time_us_64() + 50000;
        }
    } break;

    case DebouncerStep::FirstTrigger: {
        if (time_us_64() >= this->keyboard.debounceExpiry) {
            uint8_t currentKeymap = this->hw_gpioexp->GetAll(); // This clears the hw interrupt
            uint8_t persistentPress = (this->keyboard.irqFlagPoll & currentKeymap);
            this->keyboard.debounceStage = DebouncerStep::Poll;
            if (persistentPress != this->keyboard.previousPress) {
                if (persistentPress & (1 << GPIOEXP_KEY_UP)) {
                    this->keyboard.current |= KE_Up;
                }
                if (persistentPress & (1 << GPIOEXP_KEY_DOWN)) {
                    this->keyboard.current |= KE_Down;
                }
                if (persistentPress & (1 << GPIOEXP_KEY_SELECT)) {
                    this->keyboard.current |= KE_Select;
                }
                if (persistentPress & (1 << GPIOEXP_KEY_BACK)) {
                    this->keyboard.current |= KE_Back;
                }
                if (this->keyboard.current != KE_None) {
                    this->keyboard.debounceStage = DebouncerStep::PendingEvent;
                }
            }
            this->keyboard.previousPress = persistentPress;
            this->keyboard.irqFlagPoll = 0x00;
            this->keyboard.debounceExpiry = 0;
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

    if (this->standby == StandbyStage::Screensaver) {
        this->keyboard.debounceStage = DebouncerStep::Poll;
        this->keyboard.current = KE_None;
        return;
    }

    switch (this->app_currentSelect) {
    case ProgramSelect::MainMenu: {
        switch (this->keyboard.current) {
        case KE_Down: {
            if (this->app_newMenuIdx < this->ui->GetMenuSize() - 1)
                this->app_newMenuIdx++;
        } break;

        case KE_Up: {
            if (this->app_newMenuIdx > 0)
                this->app_newMenuIdx--;
        } break;

        case KE_Select: {
            this->app_newSelect = this->ui->GetMenuEntry(this->app_currentMenuIdx).first;
            this->textScroll.stage = TextScrollStep::DrawHeader;
            if (this->app_newSelect == ProgramSelect::Info) {
                this->textScroll.stage = TextScrollStep::DrawBitmap;
            }
        } break;

        default: {
            // other key strokes not expected
        } break;
        }
    } break;

    case ProgramSelect::Info: {
        switch (this->keyboard.current) {
        case KE_Down: {
            if (this->textScroll.stage == TextScrollStep::BitmapOK) {
                this->textScroll.stage = TextScrollStep::DrawHeader;
            }
        } break;

        case KE_Up: {
            if (this->textScroll.stage != TextScrollStep::BitmapOK) {
                this->textScroll.stage = TextScrollStep::DrawBitmap;
            }
        } break;

        case KE_Back: {
            this->app_newSelect = ProgramSelect::MainMenu;
            this->app_newMenuIdx = this->app_currentMenuIdx;
            this->app_currentMenuIdx = -1;
        } break;

        default: {
            // other key strokes not expected
        } break;
        }
    } break;

    default: {
        if (this->keyboard.current & KE_Back) {
            this->app_newSelect = ProgramSelect::MainMenu;
            this->logic->Stop();
            while (!queue_is_empty(&this->dataQueue)) {
                QueueData bogus;
                queue_remove_blocking(&this->dataQueue, &bogus);
            }
            this->app_newMenuIdx = this->app_currentMenuIdx;
            this->app_currentMenuIdx = -1;
        }
    } break;
    }

    this->keyboard.debounceStage = DebouncerStep::Poll;
    this->keyboard.current = KE_None;
}

void Application::UserOutput()
{
    bool drawHeader = false;
    if (this->app_newSelect != this->app_currentSelect) {
        this->ui->ClearBuffers();
        this->app_currentSelect = this->app_newSelect;
        drawHeader = true;
    }

    switch (this->app_currentSelect) {
    case ProgramSelect::MainMenu: {
        if (this->app_newMenuIdx != this->app_currentMenuIdx) {
            this->app_currentMenuIdx = this->app_newMenuIdx;
            this->ui->DrawMenu(this->app_currentMenuIdx);
        }
        if (this->standby == StandbyStage::Screensaver
            && time_us_64() - this->lastSsaverFrameChange >= spr_toaster.frameDurationMs * 1000) {
            this->ui->DrawScreenSaver(spr_toaster, this->lastSsaverFrame++ % spr_toaster.frameCount);
            this->lastSsaverFrameChange = time_us_64();
        }
    } break;

    case ProgramSelect::Info: {
        switch (this->textScroll.stage) {
        case TextScrollStep::DrawBitmap: {
            this->ui->DrawFullScreen(bmp_picoPost);
            this->ui->DrawActions(bmp_back, bmp_empty, bmp_arrowDown);
            this->textScroll.stage = TextScrollStep::BitmapOK;
        } break;

        case TextScrollStep::DrawHeader: {
            this->ui->DrawHeader("PicoPOST " PROJ_STR_VER);
            this->ui->DrawActions(bmp_back, bmp_arrowUp, bmp_empty);
            this->textScroll.sourceIdx = 0;
            this->textScroll.stage = TextScrollStep::DrawBlock;
        } break;

        case TextScrollStep::DrawBlock: {
            this->textScroll.output.clear();
            this->textScroll.output.assign(creditsLine + this->textScroll.sourceIdx);
            this->textScroll.output = this->textScroll.output.substr(0, Application::c_maxStrbuff);
            this->ui->DrawFooter(this->textScroll.output.c_str());
            this->textScroll.sourceIdx++;
            this->textScroll.sourceIdx %= strlen(creditsLine);
            this->textScroll.tick = time_us_64() + 250000;
            if (this->textScroll.sourceIdx == 0)
                this->textScroll.tick = time_us_64() + 1000000;
            this->textScroll.stage = TextScrollStep::Wait;
        } break;

        case TextScrollStep::Wait: {
            if (time_us_64() >= this->textScroll.tick)
                this->textScroll.stage = TextScrollStep::DrawBlock;
        } break;

        case TextScrollStep::Quit: {
            this->textScroll.sourceIdx = 0;
            this->textScroll.tick = 0;
        } break;

        default: {
            // other stages can simply do nothing :)
        } break;
        }
    } break;

    case ProgramSelect::UpdateFW: {
        this->ui->DrawHeader(this->ui->GetMenuEntry(this->app_currentMenuIdx).second);
        this->ui->DrawFooter("Connect to PC");
        reset_usb_boot(0, 0);
    } break;

    default: {
        if (drawHeader) {
            this->ui->DrawHeader(this->ui->GetMenuEntry(this->app_currentMenuIdx).second);
            this->ui->DrawActions(bmp_back, bmp_empty, bmp_empty);
        }

        uint count = queue_get_level(&this->dataQueue);
        if (count > 0) {
            QueueData buffer;
            queue_remove_blocking(&this->dataQueue, &buffer);
            this->lastActivityTimer = time_us_64();
            this->ui->NewData(&buffer);
        } else {
            sleep_ms(5);
        }
    } break;
    }
}

void Application::StandbyTick()
{
    uint8_t newBright = this->currBrightness;

    switch (this->standby) {
    case StandbyStage::Active: {
        if (time_us_64() - this->lastActivityTimer > c_standbyTimer) {
            this->standby = StandbyStage::Dimming;
        }
    } break;

    case StandbyStage::Dimming: {
        if (time_us_64() - this->lastActivityTimer <= c_standbyTimer) {
            newBright = c_maxBrightness;
            this->standby = StandbyStage::Active;
        } else {
            newBright = this->currBrightness - c_brightnessStep;
            if (newBright <= c_minBrightness) {
                this->standby = StandbyStage::Standby;
            }
        }
    } break;

    case StandbyStage::Standby: {
        if (this->app_currentSelect == ProgramSelect::MainMenu
            && (time_us_64() - this->lastActivityTimer > c_standbyTimer * 2)) {
            this->lastSsaverFrame = 0;
            this->lastSsaverFrameChange = time_us_64() - (spr_toaster.frameDurationMs * 2);
            this->standby = StandbyStage::Screensaver;
        } else if (time_us_64() - this->lastActivityTimer <= c_standbyTimer) {
            if (this->app_currentSelect == ProgramSelect::MainMenu) {
                this->app_newMenuIdx = this->app_currentMenuIdx;
                this->app_currentMenuIdx = -1;
            }
            newBright = c_maxBrightness;
            this->standby = StandbyStage::Active;
        }
    } break;

    default: {
        if (time_us_64() - this->lastActivityTimer <= c_standbyTimer) {
            this->app_newMenuIdx = this->app_currentMenuIdx;
            this->app_currentMenuIdx = -1;
            newBright = c_maxBrightness;
            this->standby = StandbyStage::Active;
        }
    } break;
    }

    if (this->currBrightness != newBright) {
        this->currBrightness = newBright;
        this->ui->SetScreenBrightness(this->currBrightness);
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
    queue_init(&this->dataQueue, sizeof(QueueData), MAX_QUEUE_LENGTH);

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
        printf("GPIO Exp OK! -> Assuming PCB rev6+\n");
        this->hwMode = UserMode::I2CKeypad;
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
        this->hwMode = UserMode::GPIOKeypad;*/
        BlinkenHalt(ErrorCodes::ERR_MissingGPIOExpander);
    }

    // Initialize OLED display on 1st I2C instance, @ 400 kHz, addr 0x3C
    bool dispType = false;
    bool dispSize = false;
    bool dispFlip = false;
    pico_oled::Size libOledSize = pico_oled::Size::W128xH32;
    if (this->UseNewRemote()) {
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
        this->hwMode = Serial;
        printf("OLED KO! -> Falling back to USB ACM\n");*/
        BlinkenHalt(ErrorCodes::ERR_MissingDisplay);
    }

    this->ui = new UserInterface(this->hw_oled, libOledSize);
    this->ui->ClearScreen();

    if (this->hwMode == UserMode::Serial && !stdio_usb_connected()) {
        int retry = 5;
        while (!stdio_usb_connected() && retry > 0) {
            stdio_usb_init();
            sleep_ms(100);
            retry--;
        }
        if (!stdio_usb_connected()) {
            this->hwMode = UserMode::Invalid;
        }
    }

    this->logic = std::make_unique<Logic>();

    gpio_put(PICO_DEFAULT_LED_PIN, true);
}
