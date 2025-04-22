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
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 256;
const int SCREEN_SCALE = 2;
int cycles = 0;
int scanline = 0;

/**
 * Note:
 * AI was used to figure out the basics of rendering.
 * This only applies to the process of taking frame data
 * and putting this into a window, not for the actual
 * calculation of what goes in the frame.
 */
/////////////////////////////Handle Rendering
extern unsigned char VRAM[];
SDL_Window* window;
SDL_Texture* texture;
SDL_Renderer* renderer;
SDL_Color frame_buffer[SCREEN_WIDTH*SCREEN_HEIGHT];
void setup_PPU(){
    //Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("unable to initialize SDL\n");
        exit(1);
    }

    //Create window
    window = SDL_CreateWindow("Emulate Ds",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH * SCREEN_SCALE, SCREEN_HEIGHT * SCREEN_SCALE, SDL_WINDOW_SHOWN);

    if (!window) {
        printf("unable to create window\n");
        SDL_Quit();
        exit(1);
    }

    //Create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("unable to create renderer\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        exit(1);
    }

    //temp
    SDL_Color colors[4];
    colors[0] = (SDL_Color){0,0,0,0};
    colors[1] = (SDL_Color){80,80,80,255};
    colors[2] = (SDL_Color){160,160,160,255};
    colors[3] = (SDL_Color){255,255,255,255};
    for(int r=0; r<32; r++){
        for(int c=0; c<16; c++){
            for(int i=0; i<8; i++){
                int addr = r*8*16*2+c*16+i;
                for(int j=0; j<8; j++){
                    int ci = (((VRAM[addr+8]>>(7-j))&1)<<1)+((VRAM[addr]>>(7-j))&1);
                    frame_buffer[(r*8+i)*SCREEN_WIDTH + (c*8+j)] = colors[ci];
                }
            }
        }
    }
}
void render_frame(){
    //Clear previous frame
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    //Render each pixel as a rectangle
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        SDL_Color c = frame_buffer[i];
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    
        int x = (i % SCREEN_WIDTH) * SCREEN_SCALE;
        int y = (i / SCREEN_WIDTH) * SCREEN_SCALE;
    
        SDL_Rect rect = {x, y, SCREEN_SCALE, SCREEN_SCALE};
        SDL_RenderFillRect(renderer, &rect);
    }

    //Do render
    SDL_RenderPresent(renderer);

    //Delay for FPS
    SDL_Delay(1000/FPS);
}

/////////////////////////////Run PPU
void PPU_cycle(){
    //Increment cycle and handle end of scanline
    cycles++;
    if(cycles == 341){
        cycles = 0;
        scanline++;
    }
    
    //Handle draw to buffer
    if(scanline >= 0 && scanline < 240){
        uint16_t nametable = 0x2000 | ((PPUCTRL&0b11) << 10);
        uint16_t spritetable = ((PPUCTRL>>3)&1) << 12;
        uint16_t bgtable = ((PPUCTRL>>4)&1) << 12;
        bool render_left_bg = ((PPUMASK>>1)&1);
        bool render_left_sprite = ((PPUMASK>>2)&1);
        bool render_bg = ((PPUMASK>>3)&1);
        bool render_sprite = ((PPUMASK>>4)&1);
        //TODO
    }

    //Handle VBlank start
    else if(scanline == 241 && cycles == 1){
        PPUSTATUS |= 0b10000000;
        //NMI_signal = true;
        render_frame();
    }

    //Handle VBlank ends
    else if(scanline == 261 && cycles == 1){
        PPUSTATUS = (PPUSTATUS|0b10000000) ^ 0b10000000;
        //NMI_signal = false;
    }
}