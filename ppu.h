#pragma once

#include <chrono>
#include <SDL.h>
#include <iostream>
#include "cpu.h"

void PPU_cycle();
void setup_PPU();
void render_frame();
void write_PPUADDR(uint8_t data);
uint8_t read_PPUDATA();
void write_PPUDATA(uint8_t data);
void write_OAMADDR(uint8_t data);
uint8_t read_OAMDATA();
void write_OAMDATA(uint8_t data);
void write_OAMDMA(uint8_t data);