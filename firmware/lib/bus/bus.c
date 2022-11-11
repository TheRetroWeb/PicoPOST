/**
  *    lib/bus/bus.c
  * 
  *    Actually deal with the ISA bus!
  * 
  **/

#include "bus.h"

static const uint8_t c_inputList[] = {
    PIN_ISA_D0,
    PIN_ISA_D1,
    PIN_ISA_D2,
    PIN_ISA_D3,
    PIN_ISA_D4,
    PIN_ISA_D5,
    PIN_ISA_D6,
    PIN_ISA_D7,
    PIN_ISA_A0,
    PIN_ISA_A1,
    PIN_ISA_A2,
    PIN_ISA_A3,
    PIN_ISA_A4,
    PIN_ISA_A5,
    PIN_ISA_A6,
    PIN_ISA_A7,
    // No need to get through all 16 address lines!
    PIN_ISA_BRDY,
    PIN_ISA_RST
};

static const uint8_t c_outputList[] = {
    PIN_ADDRESS_BANK,
};

void Bus_Init() {
    // Initialize inputs
    for (int i = 0; i < sizeof(c_inputList); i++) {
        gpio_init(c_inputList[i]);
        gpio_set_dir(c_inputList[i], GPIO_IN);
    }

    // Initialize outputs
    for (int i = 0; i < sizeof(c_outputList); i++) {
        gpio_init(c_outputList[i]);
        gpio_set_dir(c_outputList[i], GPIO_OUT);
    }

    // TODO: don't be a doofus and actually init quicker by using gpio_*_mask functions!
}

uint8_t Bus_IsResetting() {
    uint8_t status = 0x00;
    status = gpio_get(PIN_ISA_RST);
    return status;
}

uint8_t Bus_IsReady() {
    uint8_t status = 0x00;
    status = gpio_get(PIN_ISA_BRDY);
    return status;
}

uint8_t Bus_GetAddressUpper() {
    gpio_put(PIN_ADDRESS_BANK, true);
    uint32_t gpioMask = gpio_get_all();
    return (gpioMask & MASK_ISA_ADDR) >> SHFT_ISA_ADDR;
}

uint8_t Bus_GetAddressLower() {
    gpio_put(PIN_ADDRESS_BANK, false);
    uint32_t gpioMask = gpio_get_all();
    return (gpioMask & MASK_ISA_ADDR) >> SHFT_ISA_ADDR;
}

uint16_t Bus_GetAddress() {
    uint16_t fullAddr = 0x0000;
    fullAddr += Bus_GetAddressLower();
    fullAddr += (Bus_GetAddressUpper() << 8);
    return fullAddr;

}

uint8_t Bus_GetData() {
    uint32_t gpioMask = gpio_get_all();
    return (gpioMask & MASK_ISA_DATA) >> SHFT_ISA_DATA;
}