#include "ppu.h"

/////////////////////////////CPU
extern uint8_t VRAM[];
extern bool NMI_signal;
extern uint8_t mem[];
extern uint8_t bank_regs[];
extern uint16_t pc;
extern uint64_t clock_cycle;

/////////////////////////////OAM
uint8_t OAM[0x100];
uint8_t OAM2[0x20];

/////////////////////////////Internal regs
bool w = false;
uint16_t t;
uint16_t v;
uint8_t x;

/////////////////////////////Special Addresses
#define PPUCTRL mem[0x2000]
#define PPUMASK mem[0x2001]
#define PPUSTATUS mem[0x2002]
#define OAMDMA mem[0x4014]

/////////////////////////////Increment handling
//Note: formulas taken from https://www.nesdev.org/wiki/PPU_scrolling#PPU_internal_registers
bool is_render;
void inc_x(){
    if((v&0x1F) == 31){
        v &= ~0x1F;
        v ^=0x400;
    } else {
        v++;
    }
}
void inc_y(){
    if((v&0x7000) != 0x7000){
        v += 0x1000;
    } else {
        v &= ~0x7000;
        int y = (v & 0x03E0) >> 5;
        if(y == 29){
            y = 0;
            v ^= 0x0800;
        } else if (y == 31){
            y = 0;
        } else {
            y += 1;
        }
        v = (v & ~0x03E0) | (y<<5);  
    }
}
void inc_v(){
    if(!is_render){
        v += 1<<(((PPUCTRL>>2)&1)*5);
    } else {
        inc_x();
        inc_y();
    }
}

/////////////////////////////Special Address Handling
//PPU mapping
void write_PPUADDR(uint8_t data){
    if(!w) {
        t &= ~0xFF00;
        t |= (data&0x3F)<<8;
    } else {
        t &= ~0xFF;
        t |= data;
        v = t;
    }
    w = !w;
}
uint8_t read_PPUDATA_buffer = 0;
uint8_t read_PPUDATA(){
    if(v >= 0x3F00){
        read_PPUDATA_buffer = VRAM[v];
        uint8_t res = VRAM[VRAM_addr(v)];
        inc_v();
        return res;
    }
    uint8_t res = read_PPUDATA_buffer;
    read_PPUDATA_buffer = VRAM[VRAM_addr(v)];
    inc_v();
    return res;
}
void write_PPUDATA(uint8_t data){
    VRAM[VRAM_addr(v)] = data;
    inc_v();
}
//OAM mapping
uint16_t oamaddr;
void write_OAMADDR(uint8_t data){
    oamaddr = data;
}
uint8_t read_OAMDATA(){
    uint8_t res = OAM[oamaddr];
    return res;
}
void write_OAMDATA(uint8_t data){
    OAM[oamaddr] = data;
    oamaddr += 1;
}
void write_OAMDMA(uint8_t data){
    memcpy(&OAM[oamaddr], &mem[data<<8], 256 * sizeof(uint8_t));
}
//Scroll handling
void write_PPUSCROLL(uint8_t data){
    if(!w) {
        t &= ~0x1F;
        t |= data>>3;
        x = data & 0b111;
    } else {
        t &= ~0x73E0;
        t |= (data>>3)<<5;
        t |= (data&0b111)<<12;
    }
    w = !w;
}
void write_PPUCTRL(uint8_t data){
    t &= ~0xC00;
    t |= (data&0b11)<<10;
}

/////////////////////////////Tracking vars
bool DEBUG_SCREEN = true;
bool GRAYSCALE = false;
const int FPS = 60;
const int SCREEN_WIDTH = 256;
const int SCREEN_HEIGHT = 240;
const int SCREEN_SCALE = 3;
int cycles = 0;
int scanline = 261;

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
SDL_Rect srcRect = {0, 8, SCREEN_WIDTH, (SCREEN_HEIGHT-16)};
SDL_Rect destRect = {0, 0, SCREEN_WIDTH*SCREEN_SCALE, (SCREEN_HEIGHT-16)*SCREEN_SCALE};
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
        SCREEN_WIDTH * SCREEN_SCALE, (SCREEN_HEIGHT-16) * SCREEN_SCALE, SDL_WINDOW_SHOWN);

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
    SDL_RenderCopy(renderer, texture, &srcRect, &destRect);

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
#define NMI_enabled (PPUCTRL>>7)
#define should_render_sprite ((PPUMASK>>4)&1)
#define should_render_bg ((PPUMASK>>3)&1)
#define sprite_size ((PPUCTRL>>5)&1 ? 16 : 8)
#define sprite_switch ((PPUCTRL>>5)&1)
#define show_left_bg ((PPUMASK>>1)&1)
#define show_left_sprite ((PPUMASK>>2)&1)
//Address finders
#define sprite_pattern_address(tile_index, fine_y) \
    sprite_switch ? /* 8x16 mode */ \
        ((((tile_index&1)<<12) | ((tile_index&0xFE)<<4) | \
        ((fine_y<8) ? fine_y : (fine_y-8)) | \
        ((fine_y>=8) ? 0x10 : 0)) & 0xFFFF) : \
        ((sprite_pattern_table_index<<12) | (tile_index<<4) | ((fine_y)&0b111)) // normal 8x8 mode


#define bg_pattern_address(i, fine_y) ((bg_pattern_table_index<<12) | (i<<4) | ((fine_y)&0b111))
#define attribute_palette_index(dat, i) (((dat)>>((i)<<1))&0b11)        
//Note: the following two macros were taken from https://www.nesdev.org/wiki/PPU_scrolling#PPU_internal_registers
#define nametable_address (0x2000 | (v & 0x0FFF))
#define attribute_address (0x23C0 | (v & 0x0C00) | ((v>>4) & 0x38) | ((v>>2) & 0x07))
#define color_address(palette, i, type) (0x3f00 | ((type)<<4) | ((palette)<<2) | (i))

/////////////////////////////Mirroring
#define mirroring_layout (bank_regs[0]&0b11)
uint16_t VRAM_addr(uint16_t addr){
    uint16_t new_addr = addr;
    switch(mirroring_layout){
        case 0:
            //one-screen low
            if(addr >= 0x2400 && addr <= 0x3000) new_addr = 0x2000 | (addr%0x400);
            break;
        case 1:
            //one-screen high
            if(addr >= 0x2000 && addr <= 0x3000) new_addr = 0x2400 | (addr%0x400);
            break;
        case 2:
            //horizontal
            if(addr >= 0x2400 && addr < 0x2800) new_addr = addr-0x400;
            if(addr >= 0x2C00 && addr < 0x3000) new_addr = addr-0x400;
            break;
        case 3:
            //vertical
            if(addr >= 0x2800 && addr < 0x3000) new_addr = addr-0x800;
            break;
    }
    if(addr >= 0x3F20) {
        new_addr = 0x3F00 | (addr%0x20);
    }
    if(new_addr == 0x3F10) new_addr = 0x3F00;
    return new_addr;
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

    //Determine whether rendering
    if((scanline >=0 && scanline <= 239) || scanline == 261){
        is_render = should_render_bg || should_render_sprite;
    } else {
        is_render = false;
    }
    
    //Handle draw to buffer
    if(scanline >= 0 && scanline <= 239){
        //Render
        if(cycles >= 1 && cycles <= 256){
            //Get sprite pixel
            uint32_t sprite_color;
            bool sprite_transparent = true;
            bool sprite_unpriority;
            bool is_sprite_0;

            if(should_render_sprite == 1 && scanline > 0 && (cycles > 8 || show_left_sprite==1)){
                uint x = cycles-1;
                uint y = scanline-1;
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
            }
            
            //Get bg pixel
            uint32_t bg_color;
            bool bg_transparent = true;
            if(should_render_bg == 1 && (cycles > 8 || show_left_bg==1)){
                //Read nametable (tile address)
                int dot = cycles-1;
                uint8_t tile_addr = VRAM[VRAM_addr(nametable_address)];

                //Read pattern (using tile address)
                uint8_t tile_data_lo = VRAM[bg_pattern_address(tile_addr, (v&0x7000)>>12)];
                uint8_t tile_data_hi = VRAM[bg_pattern_address(tile_addr, (v&0x7000)>>12)|0b1000];
                uint8_t fine_x = 7-((dot+x)%8);
                int palette_index = (((tile_data_hi>>fine_x)&1)<<1) + ((tile_data_lo>>fine_x)&1);
                if(palette_index != 0) bg_transparent = false;

                //Read attribute + index within
                uint8_t palette_data = VRAM[VRAM_addr(attribute_address)];
                int palette_section = ((((v>>5)&0x1F)&2) | ((v&0x1F)&2)>>1);
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
            frame_buffer[scanline*SCREEN_WIDTH + cycles-1] = pixel_color;// / (((x>>5)%2 != (y>>5)%2)?2:1) / (((x>>4)%2 != (y>>4)%2)?3:1);
        }
        //Fill OAM2 once
        else if(cycles == 257) {
            /* if(!hit_this_frame && scanline == 239) {
                //PPUSTATUS |= 0b01000000;
                //hit_this_frame = true;
            } */
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
        if(cycles == 320){
            x_scroll = hold_x_scroll;
        }
    }

    //Handle VBlank start
    else if(scanline == 241 && cycles == 1){
        if(NMI_enabled){
            NMI_signal = true;
        }
        y_scroll = hold_y_scroll;
        PPUSTATUS |= 0b10000000; //enable VBlank
        render_frame();
        if(DEBUG_SCREEN) std::fill(frame_buffer, frame_buffer+SCREEN_HEIGHT*SCREEN_WIDTH, 0);
    }

    //Handle reset
    else if(scanline == 261){
        if(cycles == 0) {
            nametable_index = PPUCTRL&0b11;
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

    //Handle v
    if(is_render){
        if(cycles == 256){
            inc_y();
        } else if(cycles == 257){
            v &= ~0x41F;
            v |= t&0x41F;
        }
        if(cycles <= 256 && (cycles+x)%8==0 && cycles!=0){
            inc_x();
        }
        if(scanline == 261){
            if(cycles >= 280 && cycles <= 304){
                v &= 0x41F;
                v |= t&~0x41F;
            }
        }
    }
}