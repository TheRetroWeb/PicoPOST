#include <stdio.h>
#include "pico/stdlib.h"
#include "oled.h"
#include "hardware/gpio.h"

int main() {
    stdio_init_all();
    OLED_Init();    
    OLED_ClearScreen();
    
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    printf("Hello, world!\n");        
    OLED_PrintLine("!#%%&", Font_7x8, 0);

    while(1) {
        sleep_ms(1);
    }

    return 0;
}