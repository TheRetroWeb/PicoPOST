/**
  *    src/main.c
  * 
  *    Well, main!
  * 
  **/

#include "proj.h"

#include <stdio.h>
#include "pico/stdlib.h"

//#include "oled.h"
#include "bus.h"

int main() {
    // Local variables
    uint16_t currAddress = 0x0000;
    uint8_t currData = 0x00;
    uint8_t firstReset = 0x00;

    // HW/FW initialization
    stdio_init_all();
    //OLED_Init();
    //OLED_ClearScreen();
    Bus_Init();
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, true);    

    // Greetings!
    printf("-- PicoPOST v%d.%d.%d --\n", PROJ_MAJ_VER, PROJ_MIN_VER, PROJ_CDB_VER);
    printf("HW by fire219, SW by Zago\n");

    // Readout loop
    while(1) {
        // Skip readout loop while bus is resetting
        // Also reset our internal data
        while (Bus_IsResetting()) {
            if (!firstReset) {
                printf("! Resetting\n");
                firstReset = 1;
            }
            currAddress = 0x0000;
            currData = 0x00;
        }
        firstReset = 0;

        // If upper A[16..19] are low and ~IOW and ~AEN are active, read upper 8 bits
        if (Bus_IsReady()) {
            // If upper 8 bits are 0x00, read lower 8 bits
            if (Bus_GetAddressUpper() == 0x00) {
                // If lower 8 bits are 0x80, we're in business
                if (Bus_GetAddressLower() == 0x80) {
                    uint8_t newData = Bus_GetData();
                    if (newData != currData) {
                        currData = newData;
                        printf("> New code, %02Xh\n", currData);
                    }
                }
            }
        }

        // TODO Made Bus_Ready an interrupt. Polling doesn't sound very precise

        // Rinse and repeat
    }

    return 0;
}