#pragma once

#include "cpu.h"

void enable_polling(uint8_t data);
uint8_t read_controller1();
uint8_t read_controller2();
void poll();