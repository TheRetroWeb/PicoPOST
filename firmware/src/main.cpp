/**
 *    src/main.c
 *
 *    Do all the things!
 *
 **/

#include "proj.h"

#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include <stdio.h>

#include "fastread.pio.h"
#include "main.h"
#include "shapeRenderer/ShapeRenderer.h"
#include "ssd1306.h"
#include "textRenderer/TextRenderer.h"
#include "voltmon.h"

using namespace pico_ssd1306;

queue_t postList;
static volatile bool quitLoop = false;
static volatile uint64_t lastReset = 0;
SSD1306* display = nullptr;
char oldDataStr[DISPLAY_TEXT_BUFFER] = { '\0' };
char dataStr[DISPLAY_TEXT_BUFFER] = { '\0' };

void WriteSerial()
{
    while (!quitLoop) {
        int count = queue_get_level(&postList);
        if (count > 0) {
            
            QueueData buffer = {};
            queue_remove_blocking(&postList, &buffer);
            fillRect(display, 0, 12, 127, 31, WriteMode::SUBTRACT);
            switch (buffer.operation) {

            case QO_Volts: {
                double tstamp = buffer.timestamp / 1000.0;
                printf("%10.3f | %2d V @ %2.3f V\n",
                    tstamp, buffer.address, buffer.volts);
            } break;

            case QO_Data: {
                double tstamp = buffer.timestamp / 1000.0;
                memcpy(oldDataStr, dataStr, DISPLAY_TEXT_BUFFER);
                sprintf(dataStr, "%02X", buffer.data);
                printf("%10.3f | %s @ %04Xh\n",
                    tstamp, dataStr, buffer.address);
                drawText(display, font_8x8, oldDataStr, 24, 18);
                drawText(display, font_12x16, dataStr, 64, 12);
                display->sendBuffer();
            } break;

            case QO_Reset: {
                printf("Reset!\n");
                drawText(display, font_12x16, "Reset!", 20, 12);
                display->sendBuffer();
            } break;

            default: {
                // do nothing
            } break;
            }
        }
    }
}

void Logic_Port80Reader()
{
    uint resetOffset = pio_add_program(pio1, &Bus_FastReset_program);
    uint resetSm = pio_claim_unused_sm(pio1, true);
    Bus_FastReset_init(pio1, resetSm, resetOffset);

    uint readerOffset = pio_add_program(pio0, &Bus_FastRead_program);
    uint readerSm = pio_claim_unused_sm(pio0, true);
    Bus_FastRead_init(pio0, readerSm, readerOffset);

    uint8_t data = 0xFF;
    uint32_t fullRead = 0x0000;
    quitLoop = false;
    multicore_launch_core1(WriteSerial);
    lastReset = time_us_64();
    while (!quitLoop) {
        // If reset is active, this read operation blocks the loop until reset
        // goes low. Returns true only if a proper reset pulse occurred.
        // Returns false if no reset event is happening
        bool reset = Bus_FastReset_BlockingRead(pio1, resetSm);
        if (reset) {
            lastReset = time_us_64();
            QueueData qd = {
                QO_Reset,
                lastReset,
                0,
                0,
                0.0
            };
            queue_try_add(&postList, &qd);
        }

        // The readout from the FIFO should look a bit like this
        // |  A[7:0]  |  D[7:0]  |  A[15:8]  |  DC  |
        fullRead = Bus_FastRead_PinImage(pio0, readerSm);
        uint16_t addr = (fullRead & 0x0000FF00) + ((fullRead & 0xFF000000) >> 24);
        if (addr == 0x0080) {
            uint8_t data = (fullRead & 0x00FF0000) >> 16;
            QueueData qd = {
                QO_Data,
                time_us_64() - lastReset,
                addr,
                data,
                0.0
            };
            queue_try_add(&postList, &qd);
        }
    }

    pio_sm_set_enabled(pio0, readerSm, false);
    pio_sm_restart(pio0, readerSm);
    pio_sm_unclaim(pio0, readerSm);
    pio_remove_program(pio0, &Bus_FastRead_program, readerOffset);

    pio_sm_set_enabled(pio1, resetSm, false);
    pio_sm_restart(pio1, resetSm);
    pio_sm_unclaim(pio1, resetSm);
    pio_remove_program(pio1, &Bus_FastReset_program, resetOffset);
}

void Logic_VoltageMonitor()
{
    gpio_init(PICO_SMPS_MODE_PIN);
    gpio_set_dir(PICO_SMPS_MODE_PIN, GPIO_OUT);
    gpio_put(PICO_SMPS_MODE_PIN, true);

    VoltMon_Init();

    QueueData qd = {
        .operation = QO_Volts
    };
    quitLoop = false;
    multicore_launch_core1(WriteSerial);
    lastReset = time_us_64();

    while (!quitLoop) {
        float readFive = VoltMon_Read5();
        float readTwelve = VoltMon_Read12();

        uint64_t tstamp = time_us_64() - lastReset;
        qd.address = 5;
        qd.timestamp = tstamp;
        qd.volts = readFive / 1000;
        queue_try_add(&postList, &qd);
        qd.address = 12;
        qd.timestamp = tstamp;
        qd.volts = readTwelve / 1000;
        queue_try_add(&postList, &qd);

        sleep_ms(100);
    }

    gpio_put(PICO_SMPS_MODE_PIN, false);
}

int main()
{
    // onboard LED shows if we're ready for operation
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, false);

    // Init work variables
    ProgramSelect logicSelect = PS_None;
    queue_init(&postList, sizeof(QueueData), MAX_QUEUE_LENGTH);

    // Init UART output as USB CDC gadget
    stdio_init_all();

    // Init I2C bus for OLED display
    i2c_init(i2c0, 400000); // Let's keep it reasonable @ 400 kHz
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_I2C_SDA);
    gpio_pull_up(PIN_I2C_SCL);

    // Init OLED display
    display = new SSD1306(i2c0, 0x3C, Size::W128xH32);

    // Greetings!
    char strVer[13] = { '\0' };
    sprintf(strVer, "v%d.%d.%d", PROJ_MAJ_VER, PROJ_MIN_VER, PROJ_CDB_VER);
    printf("-- PicoPOST %s --\n", strVer);
    printf("HW by fire219, SW by Zago\n");
    drawText(display, font_12x16, "PicoPOST", 0, 0);
    drawText(display, font_8x8, strVer, 10, 20);
    display->sendBuffer();

    sleep_ms(2000);
    display->clear();
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    while (1) {
        // TODO: implement ui selection for use mode
        // should involve oled and buttons
        logicSelect = PS_Port80Reader;

        // pressing a certain key combo should also trigger a logic exit condition

        switch (logicSelect) {

        case PS_Port80Reader: {
            drawText(display, font_8x8, "Port 80h", 0, 0);
            display->sendBuffer();
            Logic_Port80Reader();
        } break;

        case PS_VoltageMonitor: {
            Logic_VoltageMonitor();
        } break;

        default: {
            // come again?
        } break;
        }
    }

    return 0;
}