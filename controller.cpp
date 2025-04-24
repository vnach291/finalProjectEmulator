#include "controller.h"

/////////////////////////////Polling Control
int controller1_index = 0;
int controller2_index = 0;
bool enabled = false;
void enable_polling(uint8_t data){
    if(data == 1) {
        enabled = true;
    } else if (data == 0){
        enabled = false;
        controller1_index = 0;
        controller2_index = 0;
    }
}

/////////////////////////////Polling
#define A1 SDL_SCANCODE_L
#define B1 SDL_SCANCODE_K
#define Select1 SDL_SCANCODE_T
#define Start1 SDL_SCANCODE_Y
#define Up1 SDL_SCANCODE_W
#define Down1 SDL_SCANCODE_S
#define Left1 SDL_SCANCODE_A
#define Right1 SDL_SCANCODE_D
#define A2 SDL_SCANCODE_L
#define B2 SDL_SCANCODE_K
#define Select2 SDL_SCANCODE_T
#define Start2 SDL_SCANCODE_Y
#define Up2 SDL_SCANCODE_W
#define Down2 SDL_SCANCODE_S
#define Left2 SDL_SCANCODE_A
#define Right2 SDL_SCANCODE_D
/**
 * Note: The following website was used to understand how to use SDL for polling
 * https://emudev.de/nes-emulator/implementing-controls/
 */
uint8_t controller1_reg = 0;
uint8_t controller2_reg = 0;
void poll(){
    //Exit if not polling
    if(!enabled) return;
    //Read values
    uint8_t* keys = (uint8_t*)SDL_GetKeyboardState(NULL);
    controller1_reg = 0;
    if(keys[A1]) controller1_reg |= 0b00000001;
    if(keys[B1]) controller1_reg |= 0b00000010;
    if(keys[Select1]) controller1_reg |= 0b00000100;
    if(keys[Start1]) controller1_reg |= 0b00001000;
    if(keys[Up1]) controller1_reg |= 0b00010000;
    if(keys[Down1]) controller1_reg |= 0b00100000;
    if(keys[Left1]) controller1_reg |= 0b01000000;
    if(keys[Right1]) controller1_reg |= 0b10000000;
    controller2_reg = 0;
    if(keys[A2]) controller2_reg |= 0b00000001;
    if(keys[B2]) controller2_reg |= 0b00000010;
    if(keys[Select2]) controller2_reg |= 0b00000100;
    if(keys[Start2]) controller2_reg |= 0b00001000;
    if(keys[Up2]) controller2_reg |= 0b00010000;
    if(keys[Down2]) controller2_reg |= 0b00100000;
    if(keys[Left2]) controller2_reg |= 0b01000000;
    if(keys[Right2]) controller2_reg |= 0b10000000;
}

/////////////////////////////Reading
uint8_t read_controller1(){
    if(controller1_index < 8){
        uint8_t res = (controller1_reg>>controller1_index)&1;
        controller1_index++;
        return res;
    } else {
        return 1;
    }
}
uint8_t read_controller2(){
    if(controller2_index < 8){
        uint8_t res = (controller2_reg>>controller2_index)&1;
        controller2_index++;
        return res;
    } else {
        return 1;
    }
}