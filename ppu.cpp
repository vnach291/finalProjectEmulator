#include "ppu.h"

/////////////////////////////CPU
extern unsigned char VRAM[];
extern bool NMI_signal;
extern unsigned char mem[];
extern uint64_t clock_cycle;

/////////////////////////////Internal regs
bool w = false;

/////////////////////////////Special Addresses
#define PPUCTRL mem[0x2000]
#define PPUMASK mem[0x2001]
#define PPUSTATUS mem[0x2002]
#define OAMADDR mem[0x2003]
#define OAMDATA mem[0x2004]
#define PPUSCROLL mem[0x2005]
#define OAMDMA mem[0x2008]

/////////////////////////////Special Address Handling
uint16_t ppuaddr;
void write_PPUADDR(uint8_t data){
    if(!w) {
        ppuaddr = (ppuaddr & 0x00ff) | (((uint16_t)data)<<8);
    } else {
        ppuaddr = (ppuaddr & 0xff00) | ((uint16_t)data);
    }
    w = !w;
}
uint8_t read_PPUADDR(){
    return VRAM[ppuaddr];
}
void write_PPUDATA(uint8_t data){
    VRAM[ppuaddr] = data;
    ppuaddr += 1<<(((PPUCTRL>>2)&1)*5);
}

/////////////////////////////Tracking vars
const int FPS = 60;
const int SCREEN_WIDTH = 256;
const int SCREEN_HEIGHT = 240;
const int SCREEN_SCALE = 3;
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
SDL_Window* window;
SDL_Texture* texture;
SDL_Renderer* renderer;
SDL_Color frame_buffer[SCREEN_WIDTH*SCREEN_HEIGHT];
bool has_setup;
void setup_PPU(){
    has_setup = true;

    //Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("unable to initialize SDL\n");
        exit(1);
    }

    //Create window
    window = SDL_CreateWindow("emulate deez",
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
    /* SDL_Color colors[4];
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
    } */
}
void render_frame(){
    if(!has_setup) return;

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


/////////////////////////////Palette 
//Source: https://emudev.de/nes-emulator/palettes-attribute-tables-and-sprites/
//Reformatted with AI
SDL_Color PALETTE[64] = {
    {124, 124, 124, 255}, {0, 0, 252, 255}, {0, 0, 188, 255}, {68, 40, 188, 255},
    {148, 0, 132, 255}, {168, 0, 32, 255}, {168, 16, 0, 255}, {136, 20, 0, 255},
    {80, 48, 0, 255}, {0, 120, 0, 255}, {0, 104, 0, 255}, {0, 88, 0, 255},
    {0, 64, 88, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, {0, 0, 0, 255},
    
    {188, 188, 188, 255}, {0, 120, 248, 255}, {0, 88, 248, 255}, {104, 68, 252, 255},
    {216, 0, 204, 255}, {228, 0, 88, 255}, {248, 56, 0, 255}, {228, 92, 16, 255},
    {172, 124, 0, 255}, {0, 184, 0, 255}, {0, 168, 0, 255}, {0, 168, 68, 255},
    {0, 136, 136, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, {0, 0, 0, 255},
    
    {248, 248, 248, 255}, {60, 188, 252, 255}, {104, 136, 252, 255}, {152, 120, 248, 255},
    {248, 120, 248, 255}, {248, 88, 152, 255}, {248, 120, 88, 255}, {252, 160, 68, 255},
    {248, 184, 0, 255}, {184, 248, 24, 255}, {88, 216, 84, 255}, {88, 248, 152, 255},
    {0, 232, 216, 255}, {120, 120, 120, 255}, {0, 0, 0, 255}, {0, 0, 0, 255},
    
    {252, 252, 252, 255}, {164, 228, 252, 255}, {184, 184, 248, 255}, {216, 184, 248, 255},
    {248, 184, 248, 255}, {248, 164, 192, 255}, {240, 208, 176, 255}, {252, 224, 168, 255},
    {248, 216, 120, 255}, {216, 248, 120, 255}, {184, 248, 184, 255}, {184, 248, 216, 255},
    {0, 252, 252, 255}, {248, 216, 248, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}
};
SDL_Color GRAY_PALETTE[4] = {
    {0, 0, 0, 255}, {255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}
};

/////////////////////////////OAM
uint8_t OAM[0x100];
uint8_t OAM2[0x20];

/////////////////////////////Helper methods/macros
//Reg interpreters
#define sprite_pattern_table_index ((PPUCTRL>>3)&1)
#define bg_pattern_table_index ((PPUCTRL>>4)&1)
#define nametable_index (PPUCTRL&0b11)
#define NMI_enabled (PPUCTRL>>7)
#define should_render_sprite ((PPUMASK>>4)&1)
#define should_render_bg ((PPUMASK>>3)&1)
//Address finders
#define sprite_pattern_address(i, fine_y) ((sprite_pattern_table_index<<12) | (i<<4) | ((fine_y)&0b111))
#define bg_pattern_address(i, fine_y) ((bg_pattern_table_index<<12) | (i<<4) | ((fine_y)&0b111))
#define nametable_address(x, y) ((0x2000 | (nametable_index<<10)) + (((y)<<5) | (x)))
#define attribute_address(x, y) ((0x2000 | (nametable_index<<10)) + (0x3c0) + (((y)<<3) | (x)))
#define attribute_palette_index(dat, i) (((dat)>>((i)<<1))&0b11)
#define color_address(palette, i, type) (0x3f00 | ((type)<<4) | ((palette)<<2) | (i))

/////////////////////////////Run PPU
bool hit_this_frame;
bool is_odd;
void PPU_cycle(){
    //Increment cycle and handle end of scanline
    cycles++;
    if(cycles == 341){
        cycles = 0;
        scanline++;
    }
    
    //Handle draw to buffer
    if(scanline >= 0 && scanline <= 239){
        //Render
        if(cycles >= 1 && cycles <= 256){
            int x = cycles-1;
            //Get sprite pixel
            SDL_Color sprite_color;
            if(should_render_sprite == 1){
                for(int i=0; i<8; i++){
                    //TODO read from OAM
                    //TODO make actual sprite 0 collision set
                    if(!hit_this_frame){
                        PPUSTATUS |= 0b01000000; //sprite 0 hit
                        hit_this_frame = true;;
                    }
                }
            }
            //Get bg pixel
            SDL_Color bg_color;
            bool bg_transparent = false;
            if(should_render_bg == 1){
                //Read nametable (tile address)
                uint8_t tile_addr = VRAM[nametable_address(x>>3, scanline>>3)];

                //Read pattern (using tile address)
                uint8_t tile_data_lo = VRAM[bg_pattern_address(tile_addr, scanline%8)];
                uint8_t tile_data_hi = VRAM[bg_pattern_address(tile_addr, scanline%8)|0b1000];
                int palette_index = (((tile_data_hi>>(7-x%8))&1)<<1) + ((tile_data_lo>>(7-x%8))&1);
                if(palette_index == 0) bg_transparent = true;

                //Read attribute + index within
                int palette_data = VRAM[attribute_address(x>>5, scanline>>5)];
                int palette_section = (((scanline>>4)%2)<<1) + ((x>>4)%2);
                int palette_type = (palette_data>>(palette_section<<1))&0b11;

                //Get color
                int color_index = VRAM[color_address(palette_type, palette_index, 0)];
                bg_color = PALETTE[color_index];
            }
            //Draw with logic
            //TODO logic (precedence, transparency, enable, etc)
            SDL_Color pixel_color;
            if(should_render_bg && !bg_transparent){
                pixel_color = bg_color;
            } else {
                pixel_color = PALETTE[color_address(0, 0, 0)];
            }
            frame_buffer[scanline*SCREEN_WIDTH + x] = pixel_color;
        }
        //Fill OAM2 once
        else if(cycles == 257) {
            //TODO
        }
    }

    //Handle VBlank start
    else if(scanline == 241 && cycles == 1){
        if(NMI_enabled){
            NMI_signal = true;
        }
        PPUSTATUS |= 0b10000000; //enable VBlank
        render_frame();
    }

    //Handle VBlank ends
    else if(scanline == 261 && cycles == 1){
        //TODO disable based on PPUCTRL
        PPUSTATUS &= 0b01111111; //disable VBlank
    }

    //Handle reset
    else if(scanline == 261){
        //TODO odd check
        if(cycles == 340){
            scanline = 0;
            hit_this_frame = false;
        }
    }
}