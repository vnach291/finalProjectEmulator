#pragma once

#include <chrono>
#include <SDL2/SDL.h>
#include <iostream>
#include "cpu.h"

void PPU_cycle();
void setup_PPU();
void render_frame();