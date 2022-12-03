/**
 *    src/main.c
 *
 *    Well, main!
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

void WriteSerial()
{
    while (!quitLoop) {
        int count = queue_get_level(&postList);
        if (count > 0) {
            uint8_t buffer = 0x00;
            queue_remove_blocking(&postList, &buffer);
            printf("%02X ", buffer);
        }
    }
}

void Logic_Port80Reader(PIO pio)
{
    uint offset = pio_add_program(pio, &Bus_FastRead_program);
    uint sm = pio_claim_unused_sm(pio, true);
    Bus_FastRead_init(pio, sm, offset);

    uint8_t data = 0xFF;
    uint32_t fullRead = 0x0000;
    quitLoop = false;
    multicore_launch_core1(WriteSerial);
    while (!quitLoop) {
        // The readout from the FIFO should look a bit like this
        // |  A[7:0]  |  D[7:0]  |  A[15:8]  |  DC  |

        fullRead = Bus_FastRead_PinImage(pio, sm);
        uint16_t addr = (fullRead & 0x0000FF00) + ((fullRead & 0xFF000000) >> 24);
        if (addr == 0x0080) {
            uint8_t data = (fullRead & 0x00FF0000) >> 16;
            queue_try_add(&postList, &data);
        }
    }
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_restart(pio, sm);
    pio_sm_unclaim(pio, sm);
    pio_remove_program(pio, &Bus_FastRead_program, offset);
}

int main()
{
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, false);
    stdio_init_all();

    PIO pio = pio0;
    ProgramSelect logicSelect = PS_None;
    queue_init(&postList, sizeof(uint8_t), MAX_QUEUE_LENGTH);

    // Greetings!
    printf("-- PicoPOST v%d.%d.%d --\n", PROJ_MAJ_VER, PROJ_MIN_VER, PROJ_CDB_VER);
    printf("HW by fire219, SW by Zago\n");
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    while (1) {
        // TODO: implement ui selection for use mode
        // should involve oled and buttons
        logicSelect = PS_Port80Reader;

        // pressing a certain key combo should also trigger a logic exit condition
        switch (logicSelect) {
        case PS_Port80Reader: {
            Logic_Port80Reader(pio);
        } break;

        default: {
            // come again?
        } break;
        }
    }

    return 0;
}