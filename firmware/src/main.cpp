/**
 *    src/main.c
 *
 *    Do all the things!
 *
 **/

// Primary functions
#include "app.hpp"

// System libs
#include "pico/multicore.h"
#include "hardware/irq.h"

int main()
{
    sleep_ms(10);

    Application::GetInstance();
    multicore_launch_core1(Application::LogicTask);
    Application::UITask();

    return 0;
}
