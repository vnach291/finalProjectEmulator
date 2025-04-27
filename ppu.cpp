#include "ppu.h"

/////////////////////////////CPU
extern uint8_t VRAM[];
extern bool NMI_signal;
extern uint8_t mem[];
extern uint64_t clock_cycle;

/////////////////////////////OAM
uint8_t OAM[0x100];
uint8_t OAM2[0x20];

/////////////////////////////Internal regs
bool w = false;

/////////////////////////////Special Addresses
#define PPUCTRL mem[0x2000]
#define PPUMASK mem[0x2001]
#define PPUSTATUS mem[0x2002]
#define OAMDMA mem[0x4014]

/////////////////////////////Special Address Handling
//PPU mapping
uint16_t ppuaddr;
void write_PPUADDR(uint8_t data){
    if(!w) {
        ppuaddr = (ppuaddr & 0x00ff) | (((uint16_t)data)<<8);
    } else {
        ppuaddr = (ppuaddr & 0xff00) | ((uint16_t)data);
    }
    w = !w;
}
uint8_t read_PPUDATA_buffer = 0;
uint8_t read_PPUDATA(){
    if(ppuaddr >= 0x3F00){
        read_PPUDATA_buffer = VRAM[ppuaddr];
        uint8_t res = VRAM[VRAM_addr(ppuaddr)];
        ppuaddr += 1<<(((PPUCTRL>>2)&1)*5);
        return res;
    }
    uint8_t res = read_PPUDATA_buffer;
    read_PPUDATA_buffer = VRAM[VRAM_addr(ppuaddr)];
    ppuaddr += 1<<(((PPUCTRL>>2)&1)*5);
    return res;
}
void write_PPUDATA(uint8_t data){
    //if(VRAM_addr(ppuaddr) == (0x3f00 | ((0)<<4) | ((0)<<2) | (0))) printf("%02X\n", data);
    VRAM[VRAM_addr(ppuaddr)] = data;
    ppuaddr += 1<<(((PPUCTRL>>2)&1)*5);
}
//OAM mapping
uint16_t oamaddr;
void write_OAMADDR(uint8_t data){
    oamaddr = data;
}
uint8_t read_OAMDATA(){
    return OAM[oamaddr];
}
void write_OAMDATA(uint8_t data){
    OAM[oamaddr] = data;
    oamaddr += 1;
}
void write_OAMDMA(uint8_t data){
    memcpy(&OAM[oamaddr], &mem[data<<8], 256 * sizeof(uint8_t));
}
//Scroll handling
uint16_t x_scroll;
uint16_t y_scroll;
void write_PPUSCROLL(uint8_t data){
    if(!w) {
        x_scroll = data;
    } else {
        y_scroll = data;
    }
    w = !w;
}

/////////////////////////////Tracking vars
bool DEBUG_SCREEN = true;
bool GRAYSCALE = false;
const int FPS = 60;
const int SCREEN_WIDTH = 256;
const int SCREEN_HEIGHT = 240;
const int SCREEN_SCALE = 3;
int cycles = 0;
int scanline = 0;
int mirroring_layout;

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
SDL_Rect destRect = {0, 0, SCREEN_WIDTH*SCREEN_SCALE, SCREEN_HEIGHT*SCREEN_SCALE};
SDL_Renderer* renderer;
uint32_t frame_buffer[SCREEN_WIDTH*SCREEN_HEIGHT];
uint32_t frame_buffert[SCREEN_WIDTH*SCREEN_HEIGHT];
void *frame_rendered;
int pitch = SCREEN_WIDTH*4;
bool has_setup;
void setup_PPU(){
    has_setup = true;

    //Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("unable to initialize SDL\n");
        exit(1);
    }

    //Create window
    window = SDL_CreateWindow("emuLATER",
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

    //Create texture
    texture = SDL_CreateTexture(renderer,
                SDL_PIXELFORMAT_RGBA8888,
                SDL_TEXTUREACCESS_STREAMING,
                SCREEN_WIDTH, SCREEN_HEIGHT);

    if (!texture) {
        printf("unable to create texture\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        exit(1);
    }

    for(int i=0; i<256; i++){
        for(int j=0; j<128; j++){
            //Read nametable (tile address)
            int table = i >= 128 ? 0x1000: 0x0000;
            int ii = i >= 128 ? i-128: i;
            uint8_t tile_addr = (ii>>3) + ((j>>3)<<4);

            //Read pattern (using tile address)
            uint8_t tile_data_lo = VRAM[table | (tile_addr<<4) | ((j%8)&0b111)];
            uint8_t tile_data_hi = VRAM[table | (tile_addr<<4) | ((j%8)&0b111)|0b1000];
            int palette_index = (((tile_data_hi>>(7-ii%8))&1)<<1) + ((tile_data_lo>>(7-ii%8))&1);

            //Read attribute + index within
            uint32_t GRAYS[4] = {0x000000ff,0x404040ff,0x808080ff,0xc0c0c0ff};

            frame_buffert[j*256 + i] = GRAYS[palette_index];
        }
    }
}
void render_frame(){
    if(!has_setup) return;

    //Clear previous frame
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    //Render each pixel as a rectangle
    if (SDL_LockTexture(texture, NULL, &frame_rendered, &pitch) != 0) {
        printf("Failed to lock texture: %s\n", SDL_GetError());
    } else {

        uint32_t* frame_ptr = (uint32_t*)frame_rendered;

        memcpy(frame_ptr, frame_buffer, SCREEN_WIDTH * SCREEN_HEIGHT * 4);

        SDL_UnlockTexture(texture);
    }
    SDL_RenderCopy(renderer, texture, NULL, &destRect);

    //Do render
    SDL_RenderPresent(renderer);

    //Delay for FPS
    if(!DEBUG_SCREEN) SDL_Delay(1000/FPS);
}


/////////////////////////////Palette 
//Source: https://emudev.de/nes-emulator/palettes-attribute-tables-and-sprites/
//Reformatted with AI
uint32_t PALETTE[64] = {
    0x7C7C7CFF, 0x0000FCFF, 0x0000BCFF, 0x4428BCFF, 0x940084FF, 0xA80020FF, 0xA81000FF, 0x881400FF,
    0x503000FF, 0x007800FF, 0x006800FF, 0x005800FF, 0x004058FF, 0x000000FF, 0x000000FF, 0x000000FF,
    0xBCBCBCFF, 0x0078F8FF, 0x0058F8FF, 0x6844FCFF, 0xD800CCFF, 0xE40058FF, 0xF83800FF, 0xE45C10FF,
    0xAC7C00FF, 0x00B800FF, 0x00A800FF, 0x00A844FF, 0x008888FF, 0x000000FF, 0x000000FF, 0x000000FF,
    0xF8F8F8FF, 0x3CBCFCFF, 0x6888FCFF, 0x9878F8FF, 0xF878F8FF, 0xF85898FF, 0xF87858FF, 0xFCA044FF,
    0xF8B800FF, 0xB8F818FF, 0x58D854FF, 0x58F898FF, 0x00E8D8FF, 0x787878FF, 0x000000FF, 0x000000FF,
    0xFCFCFCFF, 0xA4E4FCFF, 0xB8B8F8FF, 0xD8B8F8FF, 0xF8B8F8FF, 0xF8A4C0FF, 0xF0D0B0FF, 0xFCE0A8FF,
    0xF8D878FF, 0xD8F878FF, 0xB8F8B8FF, 0xB8F8D8FF, 0x00FCFCFF, 0xF8D8F8FF, 0x000000FF, 0x000000FF
};
uint32_t GRAYS[4] = {
    0x000000FF, 0x404040FF, 0x808080FF, 0xC0C0C0FF
};
/////////////////////////////Helper methods/macros
//Reg interpreters
#define sprite_pattern_table_index ((PPUCTRL>>3)&1)
#define bg_pattern_table_index ((PPUCTRL>>4)&1)
#define nametable_index (PPUCTRL&0b11)
#define NMI_enabled (PPUCTRL>>7)
#define should_render_sprite ((PPUMASK>>4)&1)
#define should_render_bg ((PPUMASK>>3)&1)
#define sprite_size ((PPUCTRL>>5)&1 ? 16 : 8)
#define sprite_switch ((PPUCTRL>>5)&1)
#define show_left_bg ((PPUMASK>>1)&1)
#define show_left_sprite ((PPUMASK>>2)&1)
//Address finders
//#define sprite_pattern_address(i, fine_y) ((sprite_pattern_table_index<<12) | (i<<4) | ((fine_y)&0b111))
#define sprite_pattern_address(tile_index, fine_y) \
    sprite_switch ? /* 8x16 mode */ \
        ((((tile_index&1)<<12) | ((tile_index&0xFE)<<4) | \
        ((fine_y<8) ? fine_y : (fine_y-8)) | \
        ((fine_y>=8) ? 0x10 : 0)) & 0xFFFF) : \
        ((sprite_pattern_table_index<<12) | (tile_index<<4) | ((fine_y)&0b111)) // normal 8x8 mode

#define bg_pattern_address(i, fine_y) ((bg_pattern_table_index<<12) | (i<<4) | ((fine_y)&0b111))
//#define nametable_address(x, y) ((0x2000 | (nametable_index<<10)) + (((y)<<5) | (x)))
//#define attribute_address(x, y) ((0x2000 | (nametable_index<<10)) + (0x3c0) + (((y)<<3) | (x)))
#define attribute_palette_index(dat, i) (((dat)>>((i)<<1))&0b11)
#define color_address(palette, i, type) (0x3f00 | ((type)<<4) | ((palette)<<2) | (i))
uint16_t VRAM_addr(uint16_t addr){
    uint16_t new_addr = addr;
    if(mirroring_layout == 0){
        //horizontal
        if(addr >= 0x2400 && addr < 0x2800) new_addr = addr-0x400;
        if(addr >= 0x2C00 && addr < 0x3000) new_addr = addr-0x400;
    } else {
        //vertical
        if(addr >= 0x2800 && addr < 0x3000) new_addr = addr-0x800;
    }
    if(addr >= 0x3F20) {
        new_addr = 0x3F00 | (addr%0x20);
    }
    if(new_addr == 0x3F10) new_addr = 0x3F00;
    return new_addr;
}
uint16_t scrolled_nt_addr(uint8_t x, uint8_t y){
    uint16_t base = (0x2000 | (nametable_index<<10));
    if(mirroring_layout == 0){
        //horizontal
        if(x >= 32) {
            x%=32;
        }
        if(y >= 30) {
            y%=30;
            if(nametable_index&0b10) {
                base -= 0x800;
            } else {
                base += 0x800;
            }
        }
    } else {
        //vertical
        if(x >= 32) {
            x%=32;
            if(nametable_index&0b01) {
                base -= 0x400;
            } else {
                base += 0x400;
            }
        }
        if(y >= 30) {
            y%=30;
        }
    }
    return base | (y<<5) | x;
}
uint16_t scrolled_at_addr(uint8_t x, uint8_t y){
    bool post_y = y%2 == 1;
    x>>=1;
    y>>=1;
    uint16_t base = (0x2000 | (nametable_index<<10));
    if(mirroring_layout == 0){
        //horizontal
        if(x >= 8) {
            x%=8;
        }
        if(y >= 8) {
            y%=8;
            if(nametable_index&0b10) {
                base -= 0x800;
            } else {
                base += 0x800;
            }
        }
    } else {
        //vertical
        if(x >= 8) {
            x%=8;
            if(nametable_index&0b01) {
                base -= 0x400;
            } else {
                base += 0x400;
            }
        }
        if(y >= 8) {
            y%=8;
        }
    }
    bool special_wrap = false;
    /* if(y==7 && post_y) {
        y = 0;
        special_wrap = true;
    } */
    return base + 0x3c0 + ((y<<3) | x) + (special_wrap?post_y<<2:0);
}

/////////////////////////////Run PPU
bool hit_this_frame;
bool is_odd;
bool can_be_sprite_0 = false;
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
            uint x = cycles-1;
            uint effective_x = x+x_scroll;
            uint y=scanline;
            uint effective_y = y+y_scroll;
            //Get sprite pixel
            uint32_t sprite_color;
            bool sprite_transparent = true;
            bool sprite_unpriority;
            bool is_sprite_0;

            if(should_render_sprite == 1 && scanline > 0 && (x >= 8 || show_left_sprite)){
                y--;
                //Read from OAM2
                for(int i=0; i<8; i++){
                    //Read attribute
                    uint8_t attribute_data = OAM2[(i<<2) + 2];
                    int palette_type = attribute_data & 0b11;
                    sprite_unpriority = (attribute_data>>5) & 1;
                    bool flipX = ((attribute_data>>6) & 1) == 1;
                    bool flipY = ((attribute_data>>7) & 1) == 1;

                    //Get fine x/fine y
                    uint8_t sprite_x = OAM2[(i<<2) + 3];
                    uint8_t sprite_y = OAM2[(i<<2)];

                    uint fine_x = x-sprite_x;
                    if(flipX) fine_x = 7-fine_x;
                    
                    uint fine_y = y-sprite_y;
                    uint height = sprite_size;
                    if(flipY) fine_y = height - 1 - fine_y;

                    if(fine_x < 0 || fine_x >= 8) continue;
                    if(fine_y < 0 || fine_y >= height) continue;

                    //Get tile data
                    uint8_t tile_addr = OAM2[(i<<2) + 1];
                    uint8_t tile_data_lo = VRAM[sprite_pattern_address(tile_addr, fine_y)];
                    uint8_t tile_data_hi = VRAM[sprite_pattern_address(tile_addr, fine_y)|0b1000];

                    //Calc palette index
                    int palette_index = (((tile_data_hi>>(7-fine_x))&1)<<1) + ((tile_data_lo>>(7-fine_x))&1);
                    if(palette_index != 0) sprite_transparent = false;

                    //Get color
                    int color_index = VRAM[color_address(palette_type, palette_index, 1)];
                    sprite_color = PALETTE[color_index];
                    if(GRAYSCALE) sprite_color = GRAYS[palette_type];

                    //Break on success
                    if(!sprite_transparent){
                        is_sprite_0 = i==0 && can_be_sprite_0;
                        break;
                    }
                }
                y++;
            }
            
            //Get bg pixel
            uint32_t bg_color;
            bool bg_transparent = true;
            if(should_render_bg == 1 && (x >= 8 || show_left_bg)){
                //Read nametable (tile address)
                uint8_t tile_addr = VRAM[VRAM_addr(scrolled_nt_addr(effective_x>>3, effective_y>>3))];

                //Read pattern (using tile address)
                uint8_t tile_data_lo = VRAM[bg_pattern_address(tile_addr, effective_y%8)];
                uint8_t tile_data_hi = VRAM[bg_pattern_address(tile_addr, effective_y%8)|0b1000];
                int palette_index = (((tile_data_hi>>(7-effective_x%8))&1)<<1) + ((tile_data_lo>>(7-effective_x%8))&1);
                if(palette_index != 0) bg_transparent = false;

                //Read attribute + index within
                uint8_t palette_data = VRAM[VRAM_addr(scrolled_at_addr(effective_x>>4, effective_y>>4))];
                int palette_section = (((effective_y>>4)%2)<<1) + ((effective_x>>4)%2);
                int palette_type = (palette_data>>(palette_section<<1))&0b11;

                //Get color
                int color_index = VRAM[color_address(palette_type, palette_index, 0)];
                bg_color = PALETTE[color_index];
                if(GRAYSCALE) bg_color = GRAYS[palette_type];
            }
            
            //Draw with logic
            uint32_t pixel_color;
            //Universal color
            if(sprite_transparent && bg_transparent){
                pixel_color = PALETTE[VRAM[color_address(0, 0, 0)]];
                if(GRAYSCALE) pixel_color = 0xFFFFFFFF;
            } 
            //Just BG
            else if(sprite_transparent && !bg_transparent){
                pixel_color = bg_color;
            } 
            //Just sprite
            else if(!sprite_transparent && bg_transparent){
                pixel_color = sprite_color;
            }
            //Collision
            else {
                //Do sprite 0 hit
                if(is_sprite_0 && !hit_this_frame && x != 255){
                    PPUSTATUS |= 0b01000000;
                    hit_this_frame = true;
                }
                pixel_color = sprite_unpriority ? bg_color : sprite_color;
            }
            //Set color
            frame_buffer[scanline*SCREEN_WIDTH + x] = pixel_color;// / (((x>>5)%2 != (y>>5)%2)?2:1) / (((x>>4)%2 != (y>>4)%2)?3:1);
        }
        //Fill OAM2 once
        else if(cycles == 257) {
            PPUSTATUS |= 0b01000000;
            int cnt = 0;
            uint y=scanline;
            uint height = sprite_size; 
            can_be_sprite_0 = false;
            //Find first 8 sprites on the scanline and copy to OAM2
            for(int i=0; i<64; i++){
                uint8_t sprite_y = OAM[i<<2];
                uint fine_y = y-sprite_y;
                if(fine_y < 0 || fine_y >= height) continue;
                memcpy(&OAM2[cnt<<2], &OAM[i<<2], sizeof(uint8_t) * 4);
                if(i==0) can_be_sprite_0 = true;
                cnt++;
                if(cnt == 8) break;
            }
            //Clear rest
            for(int i=cnt; i<8; i++){
                OAM2[(i<<2)] = OAM2[(i<<2)+1] = OAM2[(i<<2)+2] = OAM2[(i<<2)+3] = 0xff;
            }
        }
        if(cycles >= 257 && cycles <= 320){
            oamaddr = 0;
        }
    }

    //Handle VBlank start
    else if(scanline == 241 && cycles == 1){
        if(NMI_enabled){
            NMI_signal = true;
        }
        PPUSTATUS |= 0b10000000; //enable VBlank
        render_frame();
        if(DEBUG_SCREEN) std::fill(frame_buffer, frame_buffer+SCREEN_HEIGHT*SCREEN_WIDTH, 0);
    }

    //Handle VBlank ends
    /* else if(scanline == 261 && cycles == 1){
        //TODO disable based on PPUCTRL
        PPUSTATUS &= 0b01111111; //disable VBlank
        scanline = 0;
        hit_this_frame = false;
    } */

    //Handle reset
    else if(scanline == 261){
        if(cycles == 0) {
            PPUSTATUS &= 0b00011111;
        }
        is_odd = !is_odd;
        if((cycles == 339 && is_odd && (should_render_bg || should_render_sprite)) || 
            cycles == 340){
            cycles = 0;
            scanline = 0;
            hit_this_frame = false;
        }
    }
}