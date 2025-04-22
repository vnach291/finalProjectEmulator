#include "cpu.h"

#define TEST false
#define TEST_PRINT true

/////////////////////////////Clock
uint64_t clock_cycle = 7;
int inst_cycles = 0;

/////////////////////////////PPU
uint8_t VRAM[0x4000];
extern SDL_Window* window;
extern SDL_Renderer* renderer;
extern uint8_t OAM[];
extern uint8_t OAM2[];

/////////////////////////////Memory (64KB)
uint8_t mem[0x10000];
void write_mem(uint16_t addr, uint8_t v){
    mem[addr] = v;
    switch(addr){
        case 0x2007:
            //PPUDATA
            break;
        //TODO handle other special stuff
    }
}
uint8_t read_mem(uint16_t addr){
    uint8_t res = mem[addr];
    switch(addr){
        case 0x2002:
            //PPUSTATUS
            mem[addr] &= 0b01111111;
            break;
        //TODO handle other special stuff
    }
    return res;
}

//Interrupt signals
bool NMI_signal;
bool RES_signal;
bool IRQ_signal;
bool special_interrupt;
#define NMI_addr 0xfffa
#define RES_addr 0xfffc
#define IRQ_addr 0xfffe

/////////////////////////////Registers/Flags
uint16_t pc;
uint8_t A;
uint8_t X;
uint8_t Y;
uint8_t SP = 0xfd;
uint8_t N;
uint8_t V;
uint8_t B;
uint8_t D;
uint8_t I = 1;
uint8_t Z;
uint8_t C;
#define SR ((N<<7) + (V<<6) + (B<<4) + (D<<3) + (I<<2) + (Z<<1) + (C<<0))

//Helper functions
uint16_t read_pair(uint16_t addr) {
    return (read_mem(addr+1)<<8) | read_mem(addr);
}
void push(uint8_t val) {
    mem[0x0100 + SP] = val;
    SP-=1;
}
void push_pair(uint16_t val) {
    push((uint8_t)(val>>8));
    push((uint8_t)val);
}
uint8_t pull() {
    SP+=1;
    return mem[0x0100 + SP];
}
void pull_SR() {
    uint8_t res = pull();
    N = (res>>7)&1;
    V = (res>>6)&1;
    D = (res>>3)&1;
    I = (res>>2)&1;
    Z = (res>>1)&1;
    C = (res>>0)&1;
}
uint16_t pull_pair() {
    uint16_t res = pull();
    res |= ((uint16_t) pull())<<8;
    return res;
}
void set_NZ(uint8_t val){
    N = (val>>7) ? 1 : 0;
    Z = val ? 0 : 1;
}
void do_sbc(uint8_t value) {
    uint16_t temp = A - value - (1 - C);
    uint8_t result = temp & 0xFF;

    // Set carry if no borrow occurred (A >= M + (1-C))
    C = (temp < 0x100) ? 1 : 0;

    // Set overflow if sign bit of A and result differ and A and M differ in sign
    V = ((A ^ result) & (A ^ value) & 0x80) ? 1 : 0;

    A = result;
    set_NZ(A);
}

/////////////////////////////Addressing 
uint16_t get_imm(){
    //immediate
    return mem[pc+1];
}
uint16_t get_rel(){
    //relative
    uint16_t res = pc + ((int8_t) mem[pc + 1]) + 2;
    if((res >> 8) != (pc >> 8)) {
        inst_cycles++;
    } else {
        // special_interrupt = true;
    }
    return res;
}
uint16_t get_zpg(){
    //zero page
    return mem[pc+1];
}
uint16_t get_zpg_X(){
    //zero page X index
    return (mem[pc+1] + X) & 0xFF;
}
uint16_t get_zpg_Y(){
    //zero page Y index
    return (mem[pc+1] + Y) & 0xFF;
}
uint16_t get_X_ind(){
    //indirect X pre-index
    return (mem[(mem[pc+1] + X + 1) & 0xFF]<<8) | mem[(mem[pc+1] + X) & 0xFF];
}
uint16_t get_ind_Y(){
    //indirect Y post-index
    uint16_t res1 = (mem[(mem[pc+1] + 1) & 0xFF]<<8) | mem[mem[pc+1]];
    uint16_t res2 = res1 + Y;
    if(res1>>8 != res2>>8) inst_cycles++;
    return res2;
}
uint16_t get_abs(){
    //absolute
    return (mem[pc+2]<<8) | mem[pc+1];
}
uint16_t get_abs_X(){
    //absolute X index
    uint16_t res1 = (mem[pc+2]<<8) | mem[pc+1];
    uint16_t res2 = res1 + X;
    if(res1>>8 != res2>>8) inst_cycles++;
    return res2;
}
uint16_t get_abs_Y(){
    //absolute Y index
    uint16_t res1 = (mem[pc+2]<<8) | mem[pc+1];
    uint16_t res2 = res1 + Y;
    if(res1>>8 != res2>>8) inst_cycles++;
    return res2;
}

////////////////////////////aaabbb00 Instructions
void BRK(int mode){
    switch(mode){
        case 0:
            //BRK impl
            inst_cycles += 7;
            I = 1;
            push_pair(pc+2);
            push(SR | 0b00010000);
            pc = read_pair(IRQ_addr);
            break;
        case 2:
            //PHP impl
            inst_cycles += 3;
            push(SR | 0b00110000);
            pc = pc+1;
            break;
        case 4:
            //BPL rel
            inst_cycles += 2;
            if(N == 0){
                inst_cycles += 1;
                pc = get_rel();

            } else {
                pc = pc+2;
            }
            break;
        case 6:
            //CLC impl
            inst_cycles += 2;
            C = 0;
            pc = pc+1;
            break;
    }
}
void JSR(int mode){
    uint8_t op;
    switch(mode){
        case 0:
            //JSR abs
            inst_cycles += 6;
            push_pair(pc+2);
            pc = get_abs();
            break;
        case 1:
            //BIT zpg
            inst_cycles += 3;
            op = read_mem(get_zpg());
            N = (op>>7)&1;
            V = (op>>6)&1;
            Z = ((A & op) == 0) ? 1 : 0;
            pc = pc+2;
            break;
        case 2:
            //PLP impl
            inst_cycles += 4;
            pull_SR();
            pc = pc+1;
            break;
        case 3:
            //BIT abs
            inst_cycles += 4;
            op = read_mem(get_abs());
            N = (op>>7)&1;
            V = (op>>6)&1;
            Z = ((A & op) == 0) ? 1 : 0;
            pc = pc+3;
            break;
        case 4:
            //BMI rel
            inst_cycles += 2;
            if(N == 1){
                inst_cycles += 1;
                pc = get_rel();
            } else {
                pc = pc+2;
            }
            break;
        case 6:
            //SEC impl
            inst_cycles += 2;
            C = 1;
            pc = pc+1;
            break;
    }
}
void RTI(int mode){
    switch(mode){
        case 0:
            //RTI impl
            inst_cycles += 6;
            pull_SR();
            pc = pull_pair();
            break;
        case 2:
            //PHA impl
            inst_cycles += 3;
            push(A);
            pc = pc+1;
            break;
        case 3:
            //JMP abs
            inst_cycles += 3;
            pc = get_abs();
            break;
        case 4:
            //BVC rel
            inst_cycles += 2;
            if(V == 0){
                inst_cycles += 1;
                pc = get_rel();
            } else {
                pc = pc+2;
            }
            break;
        case 6:
            //CLI impl
            inst_cycles += 2;
            I = 0;
            pc = pc+1;
            break;
    }
}
void RTS(int mode){
    uint16_t addr; 
    switch(mode){
        case 0:
            //RTS impl
            inst_cycles += 6;
            pc = pull_pair()+1;
            break;
        case 2:
            //PLA impl
            inst_cycles += 4;
            A = pull();
            set_NZ(A);
            pc = pc+1;
            break;
        case 3:
            //JMP ind
            // inst_cycles += 5;
            // //pc = read_mem(get_abs());
            // pc = read_pair(get_abs());
            // break;
            inst_cycles += 5;
            addr = get_abs();
            // Handle the 6502 JMP indirect bug at page boundaries
            if ((addr & 0xFF) == 0xFF) {
                // When address is $xxFF, read high byte from $xx00 instead of $(xx+1)00
                pc = (read_mem((addr & 0xFF00)) << 8) | read_mem(addr);
            } else {
                // Normal case
                pc = read_pair(addr);
            }
            break;
        case 4:
            //BVS rel
            inst_cycles += 2;
            if(V == 1){
                inst_cycles += 1;
                pc = get_rel();
            } else {
                pc = pc+2;
            }
            break;
        case 6:
            //SEI impl
            inst_cycles += 2;
            I = 1;
            pc = pc+1;
            break;
    }
}
void STY(int mode){
    switch(mode){
        case 1:
            //STY zpg
            inst_cycles += 3;
            write_mem(get_zpg(), Y);
            pc = pc+2;
            break;
        case 2:
            //DEY impl
            inst_cycles += 2;
            Y = Y-1;
            set_NZ(Y);
            pc = pc+1;
            break;
        case 3:
            //STY abs
            inst_cycles += 4;
            write_mem(get_abs(), Y);
            pc = pc+3;
            break;
        case 4:
            //BCC rel
            inst_cycles += 2;
            if(C == 0){
                inst_cycles += 1;
                pc = get_rel();
            } else {
                pc = pc+2;
            }
            break;
        case 5:
            //STY zpg,X
            inst_cycles += 4;
            write_mem(get_zpg_X(), Y);
            pc = pc+2;
            break;
        case 6:
            //TYA impl
            inst_cycles += 2;
            A = Y;
            set_NZ(A);
            pc = pc+1;
            break;
    }
    
}
void LDY(int mode){
    switch(mode){
        case 0:
            //LDY #
            inst_cycles += 2;
            Y = get_imm();
            set_NZ(Y);
            pc = pc+2;
            break;
        case 1:
            //LDY zpg
            inst_cycles += 3;
            Y = read_mem(get_zpg());
            set_NZ(Y);
            pc = pc+2;
            break;
        case 2:
            //TAY impl
            inst_cycles += 2;
            Y = A;
            set_NZ(Y);
            pc = pc+1;
            break;
        case 3:
            //LDY abs
            inst_cycles += 4;
            Y = read_mem(get_abs());
            set_NZ(Y);
            pc = pc+3;
            break;
        case 4:
            //BCS rel
            inst_cycles += 2;
            if(C == 1){
                inst_cycles += 1;
                pc = get_rel();
            } else {
                pc = pc+2;
            }
            break;
        case 5:
            //LDY zpg,X
            inst_cycles += 4;
            Y = read_mem(get_zpg_X());
            set_NZ(Y);
            pc = pc+2;
            break;
        case 6:
            //CLV impl
            inst_cycles += 2;
            V = 0;
            pc = pc+1;
            break;
        case 7:
            //LDY abs,X
            inst_cycles += 4;
            Y = read_mem(get_abs_X());
            set_NZ(Y);
            pc = pc+3;
            break;
    }
}
void CPY(int mode){
    uint8_t imm;
    uint8_t res;
    switch(mode){
        case 0:
            //CPY #
            inst_cycles += 2;
            imm = get_imm();
            res = Y - imm;
            set_NZ(res);
            C = (Y >= imm) ? 1 : 0;
            pc = pc+2;
            break;
        case 1:
            //CPY zpg
            inst_cycles += 3;
            imm = read_mem(get_zpg());
            res = Y - imm;
            set_NZ(res);
            C = (Y >= imm) ? 1 : 0;
            pc = pc+2;
            break;
        case 2:
            //INY impl
            inst_cycles += 2;
            Y = Y+1;
            set_NZ(Y);
            pc = pc+1;
            break;
        case 3:
            //CPY abs
            inst_cycles += 4;
            imm = read_mem(get_abs());
            res = Y - imm;
            set_NZ(res);
            C = (Y >= imm) ? 1 : 0;
            pc = pc+3;
            break;
        case 4:
            //BNE rel
            inst_cycles += 2;
            if(Z == 0){
                inst_cycles += 1;
                pc = get_rel();
            } else {
                pc = pc+2;
            }
            break;
        case 6:
            //CLD impl
            inst_cycles += 2;
            D = 0;
            pc = pc+1;
            break;
    }
}
void CPX(int mode){
    uint8_t imm;
    uint8_t res;
    switch(mode){
        case 0:
            //CPX #
            inst_cycles += 2;
            imm = get_imm();
            res = X - imm;
            set_NZ(res);
            C = (X >= imm) ? 1 : 0;
            pc = pc+2;
            break;
        case 1:
            //CPX zpg
            inst_cycles += 3;
            imm = read_mem(get_zpg());
            res = X - imm;
            set_NZ(res);
            C = (X >= imm) ? 1 : 0;
            pc = pc+2;
            break;
        case 2:
            //INX impl
            inst_cycles += 2;
            X = X+1;
            set_NZ(X);
            pc = pc+1;
            break;
        case 3:
            //CPX abs
            inst_cycles += 4;
            imm = read_mem(get_abs());
            res = X - imm;
            set_NZ(res);
            C = (X >= imm) ? 1 : 0;
            pc = pc+3;
            break;
        case 4:
            //BEQ rel
            inst_cycles += 2;
            if(Z == 1){
                uint16_t target = pc + ((int8_t) read_mem(pc + 1)) + 2;
                inst_cycles += 1;  // +1 cycle for taken branch
                
                // check page boundary crossing
                if((target >> 8) != ((pc + 2) >> 8)) {
                    inst_cycles += 1;  // +1 additional cycle for page boundary crossing
                }
                pc = target;
            } else {
                pc = pc+2;
            }
            break;
        case 6:
            //SED impl
            inst_cycles += 2;
            D = 1;
            pc = pc+1;
            break;
    }
}

////////////////////////////aaabbb01 Instructions
void ORA(int mode){
    switch(mode){
        case 0:
            //ORA X,ind
            inst_cycles += 6;
            A = A | read_mem(get_X_ind());
            set_NZ(A);
            pc += 2;
            break;
        case 1:
            //ORA zpg
            inst_cycles += 3;
            A = A | read_mem(get_zpg());
            set_NZ(A);
            pc += 2;
            break;
        case 2:
            //ORA #
            inst_cycles += 2;
            A = A | read_mem(pc+1);
            set_NZ(A);
            pc += 2;
            break;
        case 3:
            //ORA abs
            inst_cycles += 4;
            A = A | read_mem(get_abs());
            set_NZ(A);
            pc += 3;
            break;
        case 4:
            //ORA ind,Y
            inst_cycles += 5;
            A = A | read_mem(get_ind_Y());
            set_NZ(A);
            pc += 2;
            break;
        case 5:
            //ORA zpg,X
            inst_cycles += 4;
            A = A | read_mem(get_zpg_X());
            set_NZ(A);
            pc += 2;
            break;
        case 6:
            //ORA abs,Y
            inst_cycles += 4;
            A = A | read_mem(get_abs_Y());
            set_NZ(A);
            pc += 3;
            break;
        case 7:
            //ORA abs,X
            inst_cycles += 4;
            A = A | read_mem(get_abs_X());
            set_NZ(A);
            pc += 3;
            break;
    }
}
void AND(int mode){
    switch(mode){
        case 0:
            //AND X,ind
            inst_cycles += 6;
            A = A & read_mem(get_X_ind());
            set_NZ(A);
            pc += 2;
            break;
        case 1:
            //AND zpg
            inst_cycles += 3;
            A = A & read_mem(get_zpg());
            set_NZ(A);
            pc += 2;
            break;
        case 2:
            //AND #
            inst_cycles += 2;
            A = A & read_mem(pc+1);
            set_NZ(A);
            pc += 2;
            break;
        case 3:
            //AND abs
            inst_cycles += 4;
            A = A & read_mem(get_abs());
            set_NZ(A);
            pc += 3;
            break;
        case 4:
            //AND ind,Y
            inst_cycles += 5;
            A = A & read_mem(get_ind_Y());
            set_NZ(A);
            pc += 2;
            break;
        case 5:
            //AND zpg,X
            inst_cycles += 4;
            A = A & read_mem(get_zpg_X());
            set_NZ(A);
            pc += 2;
            break;
        case 6:
            //AND abs,Y
            inst_cycles += 4;
            A = A & read_mem(get_abs_Y());
            set_NZ(A);
            pc += 3;
            break;
        case 7:
            //AND abs,X
            inst_cycles += 4;
            A = A & read_mem(get_abs_X());
            set_NZ(A);
            pc += 3;
            break;
    }
}
void EOR(int mode){
    switch(mode){
        case 0:
            //EOR X,ind
            inst_cycles += 6;
            A = A ^ read_mem(get_X_ind());
            set_NZ(A);
            pc += 2;
            break;
        case 1:
            //EOR zpg
            inst_cycles += 3;
            A = A ^ read_mem(get_zpg());
            set_NZ(A);
            pc += 2;
            break;
        case 2:
            //EOR #
            inst_cycles += 2;
            A = A ^ read_mem(pc+1);
            set_NZ(A);
            pc += 2;
            break;
        case 3:
            //EOR abs
            inst_cycles += 4;
            A = A ^ read_mem(get_abs());
            set_NZ(A);
            pc += 3;
            break;
        case 4:
            //EOR ind,Y
            inst_cycles += 5;
            A = A ^ read_mem(get_ind_Y());
            set_NZ(A);
            pc += 2;
            break;
        case 5:
            //EOR zpg,X
            inst_cycles += 4;
            A = A ^ read_mem(get_zpg_X());
            set_NZ(A);
            pc += 2;
            break;
        case 6:
            //EOR abs,Y
            inst_cycles += 4;
            A = A ^ read_mem(get_abs_Y());
            set_NZ(A);
            pc += 3;
            break;
        case 7:
            //EOR abs,X
            inst_cycles += 4;
            A = A ^ read_mem(get_abs_X());
            set_NZ(A);
            pc += 3;
            break;
    }
}
void ADC(int mode){
    uint8_t operand;
    uint16_t result;
    uint8_t original_A = A;
    switch(mode){
        case 0:
            //ADC X,ind
            inst_cycles += 6;
            operand = read_mem(get_X_ind());
            result = original_A + operand + C;
            C = (result > 0xff) ? 1 : 0;
            A = result & 0xff;
            V = ((~(original_A ^ operand) & (original_A ^ A)) & 0x80) >> 7;
            set_NZ(A);
            pc += 2;
            break;
        case 1:
            //ADC zpg
            inst_cycles += 3;
            operand = read_mem(get_zpg());
            result = original_A + operand + C;
            C = (result > 0xff) ? 1 : 0;
            A = result & 0xff;
            V = ((~(original_A ^ operand) & (original_A ^ A)) & 0x80) >> 7;
            set_NZ(A);
            pc += 2;
            break;
        case 2:
            //ADC #
            inst_cycles += 2;
            operand = read_mem(pc+1);
            result = original_A + operand + C;
            C = (result > 0xff) ? 1 : 0;
            A = result & 0xff;
            V = ((~(original_A ^ operand) & (original_A ^ A)) & 0x80) >> 7;
            set_NZ(A);
            pc += 2;
            break;
        case 3:
            //ADC abs
            inst_cycles += 4;
            operand = read_mem(get_abs());
            result = original_A + operand + C;
            C = (result > 0xff) ? 1 : 0;
            A = result & 0xff;
            V = ((~(original_A ^ operand) & (original_A ^ A)) & 0x80) >> 7;
            set_NZ(A);
            pc += 3;
            break;
        case 4:
            //ADC ind,Y
            inst_cycles += 5;
            operand = read_mem(get_ind_Y());
            result = original_A + operand + C;
            C = (result > 0xff) ? 1 : 0;
            A = result & 0xff;
            V = ((~(original_A ^ operand) & (original_A ^ A)) & 0x80) >> 7;
            set_NZ(A);
            pc += 2;
            break;
        case 5:
            //ADC zpg,X
            inst_cycles += 4;
            operand = read_mem(get_zpg_X());
            result = original_A + operand + C;
            C = (result > 0xff) ? 1 : 0;
            A = result & 0xff;
            V = ((~(original_A ^ operand) & (original_A ^ A)) & 0x80) >> 7;
            set_NZ(A);
            pc += 2;
            break;
        case 6:
            //ADC abs,Y
            inst_cycles += 4;
            operand = read_mem(get_abs_Y());
            result = original_A + operand + C;
            C = (result > 0xff) ? 1 : 0;
            A = result & 0xff;
            V = ((~(original_A ^ operand) & (original_A ^ A)) & 0x80) >> 7;
            set_NZ(A);
            pc += 3;
            break;
        case 7:
            //ADC abs,X
            inst_cycles += 4;
            operand = read_mem(get_abs_X());
            result = original_A + operand + C;
            C = (result > 0xff) ? 1 : 0;
            A = result & 0xff;
            V = ((~(original_A ^ operand) & (original_A ^ A)) & 0x80) >> 7;
            set_NZ(A);
            pc += 3;
            break;
    }
}
void STA(int mode){
    switch(mode){
        case 0:
            //STA X,ind
            inst_cycles += 6;
            write_mem(get_X_ind(), A);
            pc = pc+2;
            break;
            break;
        case 1:
            //STA zpg
            inst_cycles += 3;
            write_mem(get_zpg(), A);
            pc = pc+2;
            break;
        case 3:
            //STA abs
            inst_cycles += 4;
            write_mem(get_abs(), A);
            pc = pc+3;
            break;
        case 4:
            //STA ind,Y
            inst_cycles += 6;
            write_mem(get_ind_Y(), A);
            pc = pc+2;
            break;
        case 5:
            //STA zpg,X
            inst_cycles += 4;
            write_mem(get_zpg_X(), A);
            pc = pc+2;
            break;
        case 6:
            //STA abs,Y
            inst_cycles += 5;
            write_mem(get_abs_Y(), A);
            pc = pc+3;
            break;
        case 7:
            //STA abs,X
            inst_cycles += 5;
            write_mem(get_abs_X(), A);
            pc = pc+3;
            break;
    }
}
void LDA(int mode){
    switch(mode){
        case 0:
            //LDA X,ind
            inst_cycles += 6;
            A = read_mem(get_X_ind());
            set_NZ(A);
            pc = pc+2;
            break;
        case 1:
            //LDA zpg
            inst_cycles += 3;
            A = read_mem(get_zpg());
            set_NZ(A);
            pc = pc+2;
            break;
        case 2:
            //LDA #
            inst_cycles += 2;
            A = get_imm();
            set_NZ(A);
            pc = pc+2;
            break;
        case 3:
            //LDA abs
            inst_cycles += 4;
            A = read_mem(get_abs());
            set_NZ(A);
            pc = pc+3;
            break;
        case 4:
            //LDA ind,Y
            inst_cycles += 5;
            A = read_mem(get_ind_Y());
            set_NZ(A);
            pc = pc+2;
            break;
        case 5:
            //LDA zpg,X
            inst_cycles += 4;
            A = read_mem(get_zpg_X());
            set_NZ(A);
            pc = pc+2;
            break;
        case 6:
            //LDA abs,Y
            inst_cycles += 4;
            A = read_mem(get_abs_Y());
            set_NZ(A);
            pc = pc+3;
            break;
        case 7:
            //LDA abs,X
            inst_cycles += 4;
            A = read_mem(get_abs_X());
            set_NZ(A);
            pc = pc+3;
            break;
    }
}
void CMP(int mode){
    uint8_t val = 0;
    uint8_t res;
    switch(mode){
        case 0:
            //CMP X,ind
            inst_cycles += 6;
            val = read_mem(get_X_ind());
            res = A - val;
            set_NZ(res);
            C = (A >= val) ? 1 : 0;
            pc += 2;
            break;
        case 1:
            //CMP zpg
            inst_cycles += 3;
            val = read_mem(get_zpg());
            res = A - val;
            set_NZ(res);
            C = (A >= val) ? 1 : 0;
            pc += 2;
            break;
        case 2:
            //CMP #
            inst_cycles += 2;
            val = get_imm();
            res = A - val;
            set_NZ(res);
            C = (A >= val) ? 1 : 0;
            pc += 2;
            break;
        case 3:
            //CMP abs
            inst_cycles += 4;
            val = read_mem(get_abs());
            res = A - val;
            set_NZ(res);
            C = (A >= val) ? 1 : 0;
            pc += 3;
            break;
        case 4:
            //CMP ind,Y
            inst_cycles += 5;
            val = read_mem(get_ind_Y());
            res = A - val;
            set_NZ(res);
            C = (A >= val) ? 1 : 0;
            pc += 2;
            break;
        case 5:
            //CMP zpg,X
            inst_cycles += 4;
            val = read_mem(get_zpg_X());
            res = A - val;
            set_NZ(res);
            C = (A >= val) ? 1 : 0;
            pc += 2;
            break;
        case 6:
            //CMP abs,Y
            inst_cycles += 4;
            val = read_mem(get_abs_Y());
            res = A - val;
            set_NZ(res);
            C = (A >= val) ? 1 : 0;
            pc += 3;
            break;
        case 7:
            //CMP abs,X
            inst_cycles += 4;
            val = read_mem(get_abs_X());
            res = A - val;
            set_NZ(res);
            C = (A >= val) ? 1 : 0;
            pc += 3;
            break;
    }
}
void SBC(int mode){
    switch(mode){
        case 0:
            //SBC X,ind
            inst_cycles += 6;
            do_sbc(read_mem(get_X_ind()));
            pc += 2;
            break;
        case 1:
            //SBC zpg
            inst_cycles += 3;
            do_sbc(read_mem(get_zpg()));
            pc += 2;
            break;
        case 2:
            //SBC #
            inst_cycles += 2;
            do_sbc(get_imm());
            pc += 2;
            break;
        case 3:
            //SBC abs
            inst_cycles += 4;
            do_sbc(read_mem(get_abs()));
            pc += 3;
            break;
        case 4:
            //SBC ind,Y
            inst_cycles += 5;
            do_sbc(read_mem(get_ind_Y()));
            pc += 2;
            break;
        case 5:
            //SBC zpg,X
            inst_cycles += 4;
            do_sbc(read_mem(get_zpg_X()));
            pc += 2;
            break;
        case 6:
            //SBC abs,Y
            inst_cycles += 4;
            do_sbc(read_mem(get_abs_Y()));
            pc += 3;
            break;
        case 7:
            //SBC abs,X
            inst_cycles += 4;
            do_sbc(read_mem(get_abs_X()));
            pc += 3;
            break;
    }
}

////////////////////////////aaabbb10 Instructions
void ASL(int mode){
    switch(mode){
        case 1:
            //ASL zpg
            inst_cycles += 5;
            C = (read_mem(get_zpg()) >> 7) & 1;
            write_mem(get_zpg(), read_mem(get_zpg()) << 1);
            set_NZ(read_mem(get_zpg()));
            pc += 2;
            break;
        case 2:
            //ASL A
            inst_cycles += 2;
            C = (A >> 7) & 1;
            A = A << 1;
            set_NZ(A);
            pc += 1;
            break;
        case 3:
            //ASL abs
            inst_cycles += 6;
            C = (read_mem(get_abs()) >> 7) & 1;
            write_mem(get_abs(), read_mem(get_abs()) << 1);
            set_NZ(read_mem(get_abs()));
            pc += 3;
            break;
        case 5:
            //ASL zpg,X
            inst_cycles += 6;
            C = (read_mem(get_zpg_X()) >> 7) & 1;
            write_mem(get_zpg_X(), read_mem(get_zpg_X()) << 1);
            set_NZ(read_mem(get_zpg_X()));
            pc += 2;
            break;
        case 7:
            //ASL abs,X
            inst_cycles += 7;
            C = (read_mem(get_abs_X()) >> 7) & 1;
            write_mem(get_abs_X(), read_mem(get_abs_X()) << 1);
            set_NZ(read_mem(get_abs_X()));
            pc += 3;
            break;
    }
}
void ROL(int mode){
    switch(mode){
        case 1:
            //ROL zpg
            inst_cycles += 5;
            C = (read_mem(get_zpg()) >> 7) & 1;
            write_mem(get_zpg(), (read_mem(get_zpg()) << 1) + C);
            set_NZ(read_mem(get_zpg()));
            pc += 2;
            break;
        case 2:
            //ROL A
            inst_cycles += 2;
            C = (A >> 7) & 1;
            A = (A << 1) + C;
            set_NZ(A);
            pc += 1;
            break;
        case 3:
            //ROL abs
            inst_cycles += 6;
            C = (read_mem(get_abs()) >> 7) & 1;
            write_mem(get_abs(), (read_mem(get_abs()) << 1) + C);
            set_NZ(read_mem(get_abs()));
            pc += 3;
            break;
        case 5:
            //ROL zpg,X
            inst_cycles += 6;
            C = (read_mem(get_zpg_X()) >> 7) & 1;
            write_mem(get_zpg_X(), (read_mem(get_zpg_X()) << 1) + C);
            set_NZ(read_mem(get_zpg_X()));
            pc += 2;
            break;
        case 7:
            //ROL abs,X
            inst_cycles += 7;
            C = (read_mem(get_abs_X()) >> 7) & 1;
            write_mem(get_abs_X(), (read_mem(get_abs_X()) << 1) + C);
            set_NZ(read_mem(get_abs_X()));
            pc += 3;
            break;
    }
}
void LSR(int mode){
    switch(mode){
        case 1:
            //LSR zpg
            inst_cycles += 5;
            C = read_mem(get_zpg()) & 1;
            write_mem(get_zpg(), read_mem(get_zpg()) >> 1);
            set_NZ(read_mem(get_zpg()));
            pc += 2;
            break;
        case 2:
            //LSR A
            inst_cycles += 2;
            C = A & 1;
            A = A >> 1;
            set_NZ(A);
            pc += 1;
            break;
        case 3:
            //LSR abs
            inst_cycles += 6;
            C = read_mem(get_abs()) & 1;
            write_mem(get_abs(), read_mem(get_abs()) >> 1);
            set_NZ(read_mem(get_abs()));
            pc += 3;
            break;
        case 5:
            //LSR zpg,X
            inst_cycles += 6;
            C = read_mem(get_zpg_X()) & 1;
            write_mem(get_zpg_X(), read_mem(get_zpg_X()) >> 1);
            set_NZ(read_mem(get_zpg_X()));
            pc += 2;
            break;
        case 7:
            //LSR abs,X
            inst_cycles += 7;
            C = read_mem(get_abs_X()) & 1;
            write_mem(get_abs_X(), read_mem(get_abs_X()) >> 1);
            set_NZ(read_mem(get_abs_X()));
            pc += 3;
            break;
    }
}
void ROR(int mode){
    uint8_t old_C;
    switch(mode){
        case 1:
            //ROR zpg
            inst_cycles += 5;
            old_C = C;  // save old carry
            C = read_mem(get_zpg()) & 1;
            write_mem(get_zpg(), (read_mem(get_zpg()) >> 1) + (old_C << 7));
            set_NZ(read_mem(get_zpg()));
            pc += 2;
            break;
        case 2:
            //ROR A
            inst_cycles += 2;
            old_C = C;  // save old carry
            C = A & 1;
            A = (A >> 1) + (old_C << 7);
            set_NZ(A);
            pc += 1;
            break;
        case 3:
            //ROR abs
            inst_cycles += 6;
            old_C = C;  // save old carry
            C = read_mem(get_abs()) & 1;
            write_mem(get_abs(), (read_mem(get_abs()) >> 1) + (old_C << 7));
            set_NZ(read_mem(get_abs()));
            pc += 3;
            break;
        case 5:
            //ROR zpg,X
            inst_cycles += 6;
            old_C = C;  // save old carry
            C = read_mem(get_zpg_X()) & 1;
            write_mem(get_zpg_X(), (read_mem(get_zpg_X()) >> 1) + (old_C << 7));
            set_NZ(read_mem(get_zpg_X()));
            pc += 2;
            break;
        case 7:
            //ROR abs,X
            inst_cycles += 7;
            old_C = C;  // save old carry
            C = read_mem(get_abs_X()) & 1;
            write_mem(get_abs_X(), (read_mem(get_abs_X()) >> 1) + (old_C << 7));
            set_NZ(read_mem(get_abs_X()));
            pc += 3;
            break;
    }
}
void STX(int mode){
    switch(mode){
        case 1:
            //STX zpg
            inst_cycles += 3;
            write_mem(get_zpg(), X);
            pc += 2;
            break;
        case 2:
            //TXA impl
            inst_cycles += 2;
            A = X;
            set_NZ(A);
            pc += 1;
            break;
        case 3:
            //STX abs
            inst_cycles += 4;
            write_mem(get_abs(), X);
            pc += 3;
            break;
        case 5:
            //STX zpg,Y
            inst_cycles += 4;
            write_mem(get_zpg_Y(), X);
            pc += 2;
            break;
        case 6:
            //TXS impl
            inst_cycles += 2;
            SP = X;
            pc += 1;
            break;
    }
}
void LDX(int mode){
    switch(mode){
        case 0:
            //LDX #
            inst_cycles += 2;
            X = get_imm();
            set_NZ(X);
            pc += 2; 
            break;
        case 1:
            //LDX zpg
            inst_cycles += 3;
            X = read_mem(get_zpg());
            set_NZ(X);
            pc += 2;
            break;
        case 2:
            //TAX impl
            inst_cycles += 2;
            X = A;
            set_NZ(X);
            pc += 1;
            break;
        case 3:
            //LDX abs
            inst_cycles += 4;
            X = read_mem(get_abs());
            set_NZ(X);
            pc += 3; 
            break;
        case 5:
            //LDX zpg,Y
            inst_cycles += 4;
            X = read_mem(get_zpg_Y());
            set_NZ(X);
            pc += 2;
            break;
        case 6:
            //TSX impl
            inst_cycles += 2;
            X = SP; 
            set_NZ(X);
            pc += 1;
            break;
        case 7:
            //LDX abs,Y
            inst_cycles += 4;
            X = read_mem(get_abs_Y());
            set_NZ(X);
            pc += 3;
            break;
    }
}
void DEC(int mode){
    switch(mode){
        case 1:
            //DEC zpg
            inst_cycles += 5;
            write_mem(get_zpg(), read_mem(get_zpg()) - 1);
            set_NZ(read_mem(get_zpg()));
            pc += 2;
            break;
        case 2:
            //DEX impl
            inst_cycles += 2;
            X -= 1;
            set_NZ(X);
            pc += 1;
            break;
        case 3:
            //DEC abs
            inst_cycles += 6;
            write_mem(get_abs(), read_mem(get_abs()) - 1);
            set_NZ(read_mem(get_abs()));
            pc += 3;
            break;
        case 5:
            //DEC zpg,X
            inst_cycles += 6;
            write_mem(get_zpg_X(), read_mem(get_zpg_X()) - 1);
            set_NZ(read_mem(get_zpg_X()));
            pc += 2;
            break;
        case 7:
            //DEC abs,X
            inst_cycles += 7;
            write_mem(get_abs_X(), read_mem(get_abs_X()) - 1);
            set_NZ(read_mem(get_abs_X()));
            pc += 3;
            break;
    }
}
void INC(int mode){
    switch(mode){
        case 1:
            //INC zpg
            inst_cycles += 5;
            write_mem(get_zpg(), read_mem(get_zpg()) + 1);
            set_NZ(read_mem(get_zpg()));
            pc += 2;
            break;
        case 2:
            //NOP impl
            inst_cycles += 2;
            pc += 1;
            break;
        case 3:
            //INC abs
            inst_cycles += 6;
            write_mem(get_abs(), read_mem(get_abs()) + 1);
            set_NZ(read_mem(get_abs()));
            pc += 3;
            break;
        case 5:
            //INC zpg,X
            inst_cycles += 6;
            write_mem(get_zpg_X(), read_mem(get_zpg_X()) + 1);
            set_NZ(read_mem(get_zpg_X()));
            pc += 2;
            break;
        case 7:
            //INC abs,X
            inst_cycles += 7;
            write_mem(get_abs_X(), read_mem(get_abs_X()) + 1);
            set_NZ(read_mem(get_abs_X()));
            pc += 3;
            break;
    }
}

int tester=10;
/////////////////////////////Execute program
uint8_t get_processor_status() {
    return (N << 7) | (V << 6) | (1 << 5) | (B << 4) | (D << 3) | (I << 2) | (Z << 1) | C;
}

// Print instruction bytes (1-3 bytes depending on instruction length)
void print_instruction_bytes(uint16_t address) {
    printf("%04X  %02X ", address, mem[address]);
    
    int instr_len = 1;
    uint8_t opcode = read_mem(address);
    
    if ((opcode & 0x0F) == 0x00 || (opcode & 0x0F) == 0x08 ||
        (opcode & 0x0F) == 0x0A || (opcode & 0x0F) == 0x0C) {
        instr_len = (opcode & 0x0F) == 0x00 ? 2 : 
                    (opcode & 0x0F) == 0x0C ? 3 : 1;
    } else if ((opcode & 0x1F) == 0x10 || (opcode & 0x1F) == 0x18) {
        instr_len = 2;  
    } else if ((opcode & 0x1F) == 0x19 || (opcode & 0x1F) == 0x1D) {
        instr_len = 3;  
    } else {
        switch ((opcode & 0x1C) >> 2) {
            case 0: instr_len = 2; break;  // Immediate or Zero Page
            case 1: instr_len = 2; break;  // Zero Page
            case 2: instr_len = (opcode == 0xA9 || opcode == 0xA2 || opcode == 0xA0 || 
                opcode == 0x69 || opcode == 0xE9 || opcode == 0xC9 || 
                opcode == 0xE0 || opcode == 0xC0 || opcode == 0x09 || 
                opcode == 0x29 || opcode == 0x49) ? 2 : 1;
                break;   // Accumulator or Implied
            case 3: instr_len = 3; break;  // Absolute
            case 4: instr_len = 2; break;  // Indirect,Y or Zero Page,X
            case 5: instr_len = 2; break;  // Zero Page,X
            case 6: instr_len = 3; break;  // Absolute,Y
            case 7: instr_len = 3; break;  // Absolute,X
        }
    }
    
    // Print the instruction bytes
    if (instr_len > 1) printf("%02X ", mem[address + 1]);
    else printf("   ");
    
    if (instr_len > 2) printf("%02X ", mem[address + 2]);
    else printf("   ");
}


int run(){
    while(true){

        //Handle exits
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
                SDL_Quit();
                exit(0);
            }
        }

        //Do PPU updates from last instruction
        for(int i=0; i<inst_cycles*3; i++){
            PPU_cycle();
        }
        
        if(TEST_PRINT) {
            print_instruction_bytes(pc);
            printf("A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%lu\n", 
                A, X, Y, get_processor_status(), SP, clock_cycle);
        }

        //Get instruction
        unsigned char inst = read_mem(pc);
        unsigned char inst_a = inst >> 5;
        unsigned char inst_b = (inst >> 2) & 0b111;
        unsigned char inst_c = inst & 0b11;
        special_interrupt = false;

        //Set cycle cost to 0
        inst_cycles = 0;

        // handle special nops
        if (inst == 0x04 || inst == 0x44 || inst == 0x64 || inst == 0x0C ||
            inst == 0x14 || inst == 0x34 || inst == 0x54 || inst == 0x74 || 
            inst == 0xD4 || inst == 0xF4 ||
            inst == 0x1A || inst == 0x3A || inst == 0x5A || inst == 0x7A ||
            inst == 0xDA || inst == 0xFA || inst == 0x80 ||
            inst == 0x1C || inst == 0x3C || inst == 0x5C || inst == 0x7C ||
            inst == 0xDC || inst == 0xFC ||
            // Add LAX opcodes
            inst == 0xA3 || inst == 0xA7 || inst == 0xAF || 
            inst == 0xB3 || inst == 0xB7 || inst == 0xBF) {
            
            if (inst == 0x0C) {
                inst_cycles += 4;  // 4 cycle absolute NOP
                pc += 3;           
            } else if (inst == 0x1C || inst == 0x3C || inst == 0x5C || inst == 0x7C ||
                    inst == 0xDC || inst == 0xFC) {
                inst_cycles += 4;  // 4 cycle absolute,X NOPs
                pc += 3;           
            } else if (inst == 0xAF || inst == 0xBF) {
                inst_cycles += 4;  // 4 cycle absolute LAX NOPs
                pc += 3;           
            } else if (inst == 0x14 || inst == 0x34 || inst == 0x54 || inst == 0x74 || 
                    inst == 0xD4 || inst == 0xF4) {
                inst_cycles += 4;  // 4 cycle zero-page,X NOPs
                pc += 2;           
            } else if (inst == 0xA7 || inst == 0xB7) {
                inst_cycles += 3;  // 3-4 cycle zero-page LAX NOPs
                pc += 2;           
            } else if (inst == 0x1A || inst == 0x3A || inst == 0x5A || inst == 0x7A ||
                    inst == 0xDA || inst == 0xFA) {
                inst_cycles += 2;  // 2 cycle implied NOPs
                pc += 1;           
            } else if (inst == 0x80) {
                inst_cycles += 2;  // 2 cycle immediate NOP
                pc += 2;           
            } else if (inst == 0xA3 || inst == 0xB3) {
                inst_cycles += 6;  // 6 cycle indirect LAX NOPs
                pc += 2;           
            } else {
                inst_cycles += 3;  // 3 cycle zero-page NOPs
                pc += 2;           
            }
            clock_cycle += inst_cycles;
            continue;
        }
        //Perform instruction
        switch(inst_c){
            case 0:
                switch(inst_a){
                    case 0:
                        BRK(inst_b);
                        break;
                    case 1:
                        JSR(inst_b);
                        break;
                    case 2:
                        RTI(inst_b);
                        break;
                    case 3:
                        RTS(inst_b);
                        break;
                    case 4:
                        STY(inst_b);
                        break;
                    case 5:
                        LDY(inst_b);
                        break;
                    case 6:
                        CPY(inst_b);
                        break;
                    case 7:
                        CPX(inst_b);
                        break;
                }
                break;

            case 1:
                switch(inst_a){
                    case 0:
                        ORA(inst_b);
                        break;
                    case 1:
                        AND(inst_b);
                        break;
                    case 2:
                        EOR(inst_b);
                        break;
                    case 3:
                        ADC(inst_b);
                        break;
                    case 4:
                        STA(inst_b);
                        break;
                    case 5:
                        LDA(inst_b);
                        break;
                    case 6:
                        CMP(inst_b);
                        break;
                    case 7:
                        SBC(inst_b);
                        break;
                }
                break;

            case 2:
                switch(inst_a){
                    case 0:
                        ASL(inst_b);
                        break;
                    case 1:
                        ROL(inst_b);
                        break;
                    case 2:
                        LSR(inst_b);
                        break;
                    case 3:
                        ROR(inst_b);
                        break;
                    case 4:
                        STX(inst_b);
                        break;
                    case 5:
                        LDX(inst_b);
                        break;
                    case 6:
                        DEC(inst_b);
                        break;
                    case 7:
                        INC(inst_b);
                        break;
                }
                break;
        }

        // Print CPU state after executing instruction
        

        //Update instruction/clock
        clock_cycle += inst_cycles;

        //Do interrupts
        if(NMI_signal | RES_signal | (IRQ_signal && !I)){
            //Handle edge case
            if(special_interrupt) clock_cycle++;

            //Update stack
            push_pair(pc);
            push(SR);
            I = 1;

            //Do jump
            if(NMI_signal) {
                pc = read_pair(NMI_addr);
                NMI_signal = false;
            }
            if(RES_signal) {
                pc = read_pair(RES_addr);
                RES_signal = false;
            }
            if(IRQ_signal) {
                pc = read_pair(IRQ_addr);
                IRQ_signal = false;
            }
        }

        if(TEST) {
            tester-=1;
            if(tester <= 0) {
                printf(mem[0x6000] == 0x00 ? "Success\n" : "Fail\n"); 
                break; 
            }
        }
    }
    return 0;
}

/////////////////////////////ROM Loading
void loadROM(std::string file_name){
    std::ifstream file(file_name);
    char c;
    //Header (skipped)
    for(int i=0; i<0x10; i++){
        file.get(c);
    }
    //Program ROM
    for(int i=0; i<0x4000; i++){
        file.get(c);
        mem[0xc000+i] = c;
    }
    //Character ROM
    for(int i=0; i<0x2000; i++){
        file.get(c);
        VRAM[i] = c;
    }
    //Initialize pc
    pc = read_pair(RES_addr);
    //pc = 0xc000;
}


int main(int argc, char *argv[]) {

    //Load ROM
    std::string file_name = argv[1];
    loadROM(file_name);

    //Set up window
    setup_PPU();

    //Execute
    int exit_type = run();

    //Close window
    /* render_frame();
    SDL_Delay(10000);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit(); */

    return exit_type;
}
