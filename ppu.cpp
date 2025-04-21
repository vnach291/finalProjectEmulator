#include "ppu.h"

/////////////////////////////Special Addresses
extern unsigned char mem[];
#define PPUCTRL mem[0x2000]
#define PPUMASK mem[0x2001]
#define PPUSTATUS mem[0x2002]
#define OAMADDR mem[0x2003]
#define OAMDATA mem[0x2004]
#define PPUSCROLL mem[0x2005]
#define PPUADDR mem[0x2006]
#define PPUDATA mem[0x2007]
#define OAMDMA mem[0x2008]
extern bool NMI_signal;

/////////////////////////////Tracking vars
const int FPS = 60;
int cycles = 0;
int scanline = 0;

/////////////////////////////Run PPU
void PPU_cycle(){
    //Increment cycle and handle end of scanline
    cycles++;
    if(cycles == 341){
        cycles = 0;
        scanline++;
    }
    
    //Handle draw
    if(scanline >= 0 && scanline < 240){
        //TODO draw to frame
    }

    //Handle VBlank start
    else if(scanline == 241 && cycles == 1){
        PPUSTATUS |= 0b10000000;
        NMI_signal = true;
        //TODO draw frame
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
            printf("error initializing SDL: %s\n", SDL_GetError());
        }
        SDL_Window* win = SDL_CreateWindow("GAME",
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        1000, 1000, 0);
    }

    //Handle VBlank end
    else if(scanline == 261 && cycles == 1){
        PPUSTATUS = (PPUSTATUS|0b10000000) ^ 0b10000000;
        NMI_signal = false;
    }
}