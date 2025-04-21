#include <cstdint>
#include <string>

/////////////////////////////Clock
uint64_t clock_cycle = 0;

/////////////////////////////Memory (64KB)
char[0x10000] mem;
//TODO setup special addresses as macros

/////////////////////////////Registers/Flags
uint16_t pc;
uint8_t A;
uint8_t X;
uint8_t Y;
uint8_t SP;
uint8_t N;
uint8_t V;
uint8_t B;
uint8_t D;
uint8_t I;
uint8_t Z;
uint8_t C;
#define SR ((N<<7) + (V<<6) + (B<<4) + (D<<3) + (I<<2) + (Z<<1) + (C<<0))

//Interrupt signals
bool NMI_signal;
bool RES_signal;
bool IRQ_signal;
bool special_interrupt;
#define NMI_addr 0xfffa
#define RES_addr 0xfffc
#define IRQ_addr 0xfffe

//Helper functions
uint16_t read_pair(uint16_t addr) {
    return (mem[addr+1]<<8) | mem[addr];
}
void push(uint8_t val) {
    mem[SP] = val;
    SP-=1;
}
void push_pair(uint16_t val) {
    push((uint8_t)(val>>8));
    push((uint8_t)val);
}
uint8_t pull() {
    SP+=1;
    return mem[SP];
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
}
void set_NZ(uint8_t val){
    N = (val>>7) ? 1 : 0;
    Z = val ? 0 : 1;
}

/////////////////////////////Addressing 
uint16_t get_imm(){
    //immediate
    return mem[pc+1];
}
uint16_t get_rel(){
    //relative
    uint16_t res = ((int16_t)pc)+((int8_t)mem[pc+1]);
    if(res>>8 != pc>>8) {
        inst_cycles++;
    } else {
        special_interrupt = true;
    }
    return res;
}
uint16_t get_zpg(){
    //zero page
    return mem[pc+1];
}
uint16_t get_zpg_X(){
    //zero page X index
    return (mem[pc+1] + X) % 0xff;
}
uint16_t get_zpg_Y(){
    //zero page Y index
    return (mem[pc+1] + Y) % 0xff;
}
uint16_t get_X_ind(){
    //indirect X pre-index
    return (mem[(mem[pc+1] + X + 1) % 0xff]<<8) | mem[(mem[pc+1] + X) % 0xff];
}
uint16_t get_ind_Y(){
    //indirect Y post-index
    uint16_t res1 = (mem[(mem[pc+1] + 1) % 0xff]<<8) | mem[mem[pc+1]];
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


////////////////////////////aaabbb00 Instructions
void BRK(int mode){
    switch(mode){
        case 0:
            //BRK impl
            inst_cycles += 7;
            I = 1;
            push_pair(pc+2);
            push(SR | 0b00010000);
            pc = read_pair(NMI_addr);
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
            uint8_t op = mem[get_zpg()];
            N = (op>>7)&1;
            V = (op>>6)&1;
            Z = (A & op) ? 1 : 0;
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
            uint8_t op = mem[get_abs()];
            N = (op>>7)&1;
            V = (op>>6)&1;
            Z = (A & op) ? 1 : 0;
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
            inst_cycles += 5;
            pc = mem[get_abs()];
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
            mem[get_zpg()] = Y;
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
            mem[get_abs()] = Y;
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
            mem[get_zpg_X()] = Y;
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
            Y = mem[get_zpg()];
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
            Y = mem[get_abs()];
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
            Y = mem[get_zpg_X()];
            set_NZ(Y);
            pc = pc+3;
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
            Y = mem[get_abs_X()];
            set_NZ(Y);
            pc = pc+1;
            break;
    }
}
void CPY(int mode){
    switch(mode){
        case 0:
            //CPY #
            inst_cycles += 2;
            uint8_t imm = get_imm();
            uint8_t res = Y - imm;
            set_NZ(res);
            C = (Y >= imm) ? 1 : 0;
            pc = pc+2;
            break;
        case 1:
            //CPY zpg
            inst_cycles += 3;
            uint8_t imm = mem[get_zpg()];
            uint8_t res = Y - imm;
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
            uint8_t imm = mem[get_abs()];
            uint8_t res = Y - imm;
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
    switch(mode){
        case 0:
            //CPX #
            inst_cycles += 2;
            uint8_t imm = get_imm();
            uint8_t res = X - imm;
            set_NZ(res);
            C = (X >= imm) ? 1 : 0;
            pc = pc+2;
            break;
        case 1:
            //CPX zpg
            inst_cycles += 3;
            uint8_t imm = mem[get_zpg()];
            uint8_t res = X - imm;
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
            uint8_t imm = mem[get_abs()];
            uint8_t res = X - imm;
            set_NZ(res);
            C = (X >= imm) ? 1 : 0;
            pc = pc+3;
            break;
        case 4:
            //BEQ rel
            inst_cycles += 2;
            if(Z == 1){
                inst_cycles += 1;
                pc = get_rel();
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
            break;
        case 1:
            //ORA zpg
            break;
        case 2:
            //ORA #
            break;
        case 3:
            //ORA abs
            break;
        case 4:
            //ORA ind,Y
            break;
        case 5:
            //ORA zpg,X
            break;
        case 6:
            //ORA abs,Y
            break;
        case 7:
            //ORA abs,X
            break;
    }
}
void AND(int mode){
    switch(mode){
        case 0:
            //AND X,ind
            break;
        case 1:
            //AND zpg
            break;
        case 2:
            //AND #
            break;
        case 3:
            //AND abs
            break;
        case 4:
            //AND ind,Y
            break;
        case 5:
            //AND zpg,X
            break;
        case 6:
            //AND abs,Y
            break;
        case 7:
            //AND abs,X
            break;
    }
}
void EOR(int mode){
    switch(mode){
        case 0:
            //EOR X,ind
            break;
        case 1:
            //EOR zpg
            break;
        case 2:
            //EOR #
            break;
        case 3:
            //EOR abs
            break;
        case 4:
            //EOR ind,Y
            break;
        case 5:
            //EOR zpg,X
            break;
        case 6:
            //EOR abs,Y
            break;
        case 7:
            //EOR abs,X
            break;
    }
}
void ADC(int mode){
    switch(mode){
        case 0:
            //ADC X,ind
            break;
        case 1:
            //ADC zpg
            break;
        case 2:
            //ADC #
            break;
        case 3:
            //ADC abs
            break;
        case 4:
            //ADC ind,Y
            break;
        case 5:
            //ADC zpg,X
            break;
        case 6:
            //ADC abs,Y
            break;
        case 7:
            //ADC abs,X
            break;
    }
}
void STA(int mode){
    switch(mode){
        case 0:
            //STA X,ind
            inst_cycles += 6;
            mem[get_X_ind()] = A;
            pc = pc+2;
            break;
            break;
        case 1:
            //STA zpg
            inst_cycles += 3;
            mem[get_zpg()] = A;
            pc = pc+2;
            break;
        case 3:
            //STA abs
            inst_cycles += 4;
            mem[get_abs()] = A;
            pc = pc+3;
            break;
        case 4:
            //STA ind,Y
            inst_cycles += 6;
            mem[get_ind_Y()] = A;
            pc = pc+2;
            break;
        case 5:
            //STA zpg,X
            inst_cycles += 4;
            mem[get_zpg_X()] = A;
            pc = pc+2;
            break;
        case 6:
            //STA abs,Y
            inst_cycles += 5;
            mem[get_abs_Y()] = A;
            pc = pc+3;
            break;
        case 7:
            //STA abs,X
            inst_cycles += 5;
            mem[get_abs_X()] = A;
            pc = pc+3;
            break;
    }
}
void LDA(int mode){
    switch(mode){
        case 0:
            //LDA X,ind
            inst_cycles += 6;
            A = mem[get_X_ind()];
            set_NZ(A);
            pc = pc+2;
            break;
        case 1:
            //LDA zpg
            inst_cycles += 3;
            A = mem[get_zpg()];
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
            A = mem[get_abs()];
            set_NZ(A);
            pc = pc+3;
            break;
        case 4:
            //LDA ind,Y
            inst_cycles += 5;
            A = mem[get_ind_Y()];
            set_NZ(A);
            pc = pc+2;
            break;
        case 5:
            //LDA zpg,X
            inst_cycles += 4;
            A = mem[get_zpg_X()];
            set_NZ(A);
            pc = pc+2;
            break;
        case 6:
            //LDA abs,Y
            inst_cycles += 4;
            A = mem[get_abs_Y()];
            set_NZ(A);
            pc = pc+3;
            break;
        case 7:
            //LDA abs,X
            inst_cycles += 4;
            A = mem[get_abs_X()];
            set_NZ(A);
            pc = pc+3;
            break;
    }
}
void CMP(int mode){
    uint8_t val = 0;
    switch(mode){
        case 0:
            //CMP X,ind
            inst_cycles += 6;
            val = mem[get_X_ind()];
            uint8_t res = A - val;
            set_NZ(res);
            C = (A >= val) ? 1 : 0;
            pc += 2;
            break;
        case 1:
            //CMP zpg
            inst_cycles += 3;
            val = mem[get_zpg()];
            uint8_t res = A - val;
            set_NZ(res);
            C = (A >= val) ? 1 : 0;
            pc += 2;
            break;
        case 2:
            //CMP #
            inst_cycles += 2;
            val = get_imm();
            uint8_t res = A - val;
            set_NZ(res);
            C = (A >= val) ? 1 : 0;
            pc += 2;
            break;
        case 3:
            //CMP abs
            inst_cycles += 4;
            val = mem[get_abs()];
            uint8_t res = A - val;
            set_NZ(res);
            C = (A >= val) ? 1 : 0;
            pc += 3;
            break;
        case 4:
            //CMP ind,Y
            inst_cycles += 5;
            val = mem[get_ind_Y()];
            uint8_t res = A - val;
            set_NZ(res);
            C = (A >= val) ? 1 : 0;
            pc += 2;
            break;
        case 5:
            //CMP zpg,X
            inst_cycles += 4;
            val = mem[get_zpg_X()];
            uint8_t res = A - val;
            set_NZ(res);
            C = (A >= val) ? 1 : 0;
            pc += 2;
            break;
        case 6:
            //CMP abs,Y
            inst_cycles += 4;
            val = mem[get_abs_Y()];
            uint8_t res = A - val;
            set_NZ(res);
            C = (A >= val) ? 1 : 0;
            pc += 3;
            break;
        case 7:
            //CMP abs,X
            inst_cycles += 4;
            val = mem[get_abs_X()];
            uint8_t res = A - val;
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
            do_sbc(mem[get_X_ind()]);
            pc += 2;
            break;
        case 1:
            //SBC zpg
            inst_cycles += 3;
            do_sbc(mem[get_zpg()]);
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
            do_sbc(mem[get_abs()]);
            pc += 3;
            break;
        case 4:
            //SBC ind,Y
            inst_cycles += 5;
            do_sbc(mem[get_ind_Y()]);
            pc += 2;
            break;
        case 5:
            //SBC zpg,X
            inst_cycles += 4;
            do_sbc(mem[get_zpg_X()]);
            pc += 2;
            break;
        case 6:
            //SBC abs,Y
            inst_cycles += 4;
            do_sbc(mem[get_abs_Y()]);
            pc += 3;
            break;
        case 7:
            //SBC abs,X
            inst_cycles += 4;
            do_sbc(mem[get_abs_X()]);
            pc += 3;
            break;
    }
}

////////////////////////////aaabbb10 Instructions
void ASL(int mode){
    switch(mode){
        case 1:
            //ASL zpg
            break;
        case 2:
            //ASL A
            break;
        case 3:
            //ASL abs
            break;
        case 5:
            //ASL zpg,X
            break;
        case 7:
            //ASL abs,X
            break;
    }
}
void ROL(int mode){
    switch(mode){
        case 1:
            //ROL zpg
            break;
        case 2:
            //ROL A
            break;
        case 3:
            //ROL abs
            break;
        case 5:
            //ROL zpg,X
            break;
        case 7:
            //ROL abs,X
            break;
    }
}
void LSR(int mode){
    switch(mode){
        case 1:
            //LSR zpg
            break;
        case 2:
            //LSR A
            break;
        case 3:
            //LSR abs
            break;
        case 5:
            //LSR zpg,X
            break;
        case 7:
            //LSR abs,X
            break;
    }
}
void ROR(int mode){
    switch(mode){
        case 1:
            //ROR zpg
            break;
        case 2:
            //ROR A
            break;
        case 3:
            //ROR abs
            break;
        case 5:
            //ROR zpg,X
            break;
        case 7:
            //ROR abs,X
            break;
    }
}
void STX(int mode){
    switch(mode){
        case 1:
            //STX zpg
            break;
        case 2:
            //TXA impl
            break;
        case 3:
            //STX abs
            break;
        case 5:
            //STX zpg,Y
            break;
        case 6:
            //TXS impl
            break;
    }
}
void LDX(int mode){
    switch(mode){
        case 0:
            //LDX #
            break;
        case 1:
            //LDX zpg
            break;
        case 2:
            //TAX impl
            break;
        case 3:
            //LDX abs
            break;
        case 5:
            //LDX zpg,Y
            break;
        case 6:
            //TSX impl
            break;
        case 7:
            //LDX abs,Y
            break;
    }
}
void DEC(int mode){
    switch(mode){
        case 1:
            //DEC zpg
            break;
        case 2:
            //DEX impl
            break;
        case 3:
            //DEC abs
            break;
        case 5:
            //DEC zpg,X
            break;
        case 7:
            //DEC abs,X
            break;
    }
}
void INC(int mode){
    switch(mode){
        case 1:
            //INC zpg
            break;
        case 2:
            //NOP impl
            break;
        case 3:
            //INC abs
            break;
        case 5:
            //INC zpg,X
            break;
        case 7:
            //INC abs,X
            break;
    }
}

/////////////////////////////Execute program
int inst_cycles;
int run(){
    while(true){
        //Get instruction
        char inst = mem[pc];
        char inst_a = inst >> 5;
        char inst_b = (inst >> 2) & 0b111;
        char inst_c = inst & 0b11;
        special_interrupt = false;

        //Set cycle cost to 0
        inst_cycles = 0;

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

        //Update instruction/clock
        clock_cycle += inst_cycles;

        //Do interrupts
        if(NMI_signal | RES_signal | (IRQ_signal && !I)){
            //Handle edge case
            if(special_interrupt) clock_cycle++;

            //Update stack
            push_pair(pc);
            push(SR);

            //Do jump
            if(NMI_signal) pc = read_pair(NMI_addr);
            if(RES_signal) pc = read_pair(RES_addr);
            if(IRQ_signal) pc = read_pair(IRQ_addr);
        }
    }
    return 0;
}

/////////////////////////////ROM Loading
void loadROM(std::ifstream file){
    for(){
        
    }
}

int main(int argc, char *argv[]) {

    //Load ROM
    std::string file_name = argv[1];
    stf::ifstream ROM_file(file_name);

    //Execute
    return run();
}
