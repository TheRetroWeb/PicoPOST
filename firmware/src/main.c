/**
 *    src/main.c
 *
 *    Do all the things!
 *
 **/

#include "proj.h"

#include "hardware/pio.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include <stdio.h>

//#include "oled.h"
#include "fastread.pio.h"
#include "main.h"

queue_t postList;
static volatile bool quitLoop = false;
static volatile uint64_t lastReset = 0;

void WriteSerial()
{
    while (!quitLoop) {
        int count = queue_get_level(&postList);
        if (count > 0) {
            QueueData buffer = {};
            queue_remove_blocking(&postList, &buffer);
            switch (buffer.operation) {

            case QO_Data: {
                double tstamp = buffer.timestamp / 1000.0;
                printf("%10.3f | %02X @ %04Xh\n", tstamp, buffer.data, buffer.address);
            } break;

            case QO_Reset: {
                printf("Reset!\n");
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
                .operation = QO_Reset,
                .address = 0,
                .data = 0,
                .timestamp = lastReset
            };
            queue_try_add(&postList, &qd);
        }

        // The readout from the FIFO should look a bit like this
        // |  A[7:0]  |  D[7:0]  |  A[15:8]  |  DC  |
        fullRead = Bus_FastRead_PinImage(pio0, readerSm);
        uint16_t addr = (fullRead & 0x0000FF00) +
                       ((fullRead & 0xFF000000) >> 24);
        if (addr == 0x0080) {
            uint8_t data = (fullRead & 0x00FF0000) >> 16;
            QueueData qd = {
                .operation = QO_Data,
                .address = addr,
                .data = data,
                .timestamp = time_us_64() - lastReset
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

int main()
{
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, false);
    stdio_init_all();

    ProgramSelect logicSelect = PS_None;
    queue_init(&postList, sizeof(QueueData), MAX_QUEUE_LENGTH);

    // Greetings!
    printf("-- PicoPOST v%d.%d.%d --\n",
        PROJ_MAJ_VER, PROJ_MIN_VER, PROJ_CDB_VER);
    printf("HW by fire219, SW by Zago\n");
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    while (1) {
        // TODO: implement ui selection for use mode
        // should involve oled and buttons
        logicSelect = PS_Port80Reader;

        // pressing a certain key combo should also trigger a logic exit condition

        switch (logicSelect) {

        case PS_Port80Reader: {
            Logic_Port80Reader();
        } break;

        default: {
            // come again?
        } break;

        }
    }

    return 0;
}