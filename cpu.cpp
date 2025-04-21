#include <cstdint>

/////////////////////////////Clock
uint64_t clock_cycle = 0;

/////////////////////////////Memory (64KB)
char mem[0x10000];
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
#define SR (N<<7 + V<<6 + B<<4 + D<<3 + I<<2 + Z<<1 + C<<0)

//Helper functions
uint16_t read_pair(uint16_t addr) {
    return mem[addr+1]<<8 + mem[addr];
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
void pull_pair() {
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
    if(res>>8 != pc>>8) inst_cycles++;
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
    return mem[(mem[pc+1] + X + 1) % 0xff]<<8 + mem[(mem[pc+1] + X) % 0xff];
}
uint16_t get_ind_Y(){
    //indirect Y post-index
    uint16_t res1 = mem[(mem[pc+1] + 1) % 0xff]<<8 + mem[mem[pc+1]];
    uint16_t res2 = res1 + Y;
    if(res1>>8 != res2>>8) inst_cycles++;
    return res2;
}
uint16_t get_abs(){
    //absolute
    return mem[pc+2]<<8 + mem[pc+1];
}
uint16_t get_abs_X(){
    //absolute X index
    uint16_t res1 = mem[pc+2]<<8 + mem[pc+1];
    uint16_t res2 = res1 + X;
    if(res1>>8 != res2>>8) inst_cycles++;
    return res2;
}
uint16_t get_abs_Y(){
    //absolute Y index
    uint16_t res1 = mem[pc+2]<<8 + mem[pc+1] + Y;
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
            pc = read_pair(0xfffe);
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
                pc = pc+1;
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
            N = (op>>7&1);
            V = (op>>6&1);
            Z = (A & op) ? 1 : 0;
            pc = pc+1;
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
            N = (op>>7&1);
            V = (op>>6&1);
            Z = (A & op) ? 1 : 0;
            pc = pc+1;
            break;
        case 4:
            //BMI rel
            inst_cycles += 2;
            if(N == 1){
                inst_cycles += 1;
                pc = get_rel();
            } else {
                pc = pc+1;
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
                pc = pc+1;
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
            N = (A>>7) ? 1 : 0;
            Z = A ? 0 : 1;
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
                pc = pc+1;
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
            pc = pc+1;
            break;
        case 2:
            //DEY impl
            inst_cycles += 2;
            Y = Y-1;
            pc = pc+1;
            break;
        case 3:
            //STY abs
            inst_cycles += 4;
            mem[get_abs()] = Y;
            pc = pc+1;
            break;
        case 4:
            //BCC rel
            inst_cycles += 2;
            if(C == 0){
                inst_cycles += 1;
                pc = get_rel();
            } else {
                pc = pc+1;
            }
            break;
        case 5:
            //STY zpg,X
            inst_cycles += 4;
            mem[get_zpg_X()] = Y;
            pc = pc+1;
            break;
        case 6:
            //TYA impl
            break;
    }
    
}
void LDY(int mode){
    switch(mode){
        case 0:
            //LDY #
            break;
        case 1:
            //LDY zpg
            break;
        case 2:
            //TAY impl
            break;
        case 3:
            //LDY abs
            break;
        case 4:
            //BCS rel
            break;
        case 5:
            //LDY zpg,X
            break;
        case 6:
            //CLV impl
            break;
        case 7:
            //LDY abs,X
            break;
    }
}
void CPY(int mode){
    switch(mode){
        case 0:
            //CPY #
            break;
        case 1:
            //CPY zpg
            break;
        case 2:
            //INY impl
            break;
        case 3:
            //CPY abs
            break;
        case 4:
            //BNE rel
            break;
        case 6:
            //CLD impl
            break;
    }
}
void CPX(int mode){
    switch(mode){
        case 0:
            //CPX #
            break;
        case 1:
            //CPX zpg
            break;
        case 2:
            //INX impl
            break;
        case 3:
            //CPX abs
            break;
        case 4:
            //BEQ rel
            break;
        case 6:
            //SED impl
            break;
    }
}

////////////////////////////aaabbb01 Instructions
void ORA(int mode){
    switch(mode){
        case 0:
            //ORA X,ind
            inst_cycles += 6;
            A = A | mem[get_X_ind()];
            set_NZ(A);
            pc += 2;
            break;
        case 1:
            //ORA zpg
            inst_cycles += 3;
            A = A | mem[get_zpg()];
            set_NZ(A);
            pc += 2;
            break;
        case 2:
            //ORA #
            inst_cycles += 2;
            A = A | mem[pc+1];
            set_NZ(A);
            pc += 2;
            break;
        case 3:
            //ORA abs
            inst_cycles += 4;
            A = A | mem[get_abs()];
            set_NZ(A);
            pc += 3;
            break;
        case 4:
            //ORA ind,Y
            inst_cycles += 5;
            A = A | mem[get_ind_Y()];
            set_NZ(A);
            pc += 2;
            break;
        case 5:
            //ORA zpg,X
            inst_cycles += 4;
            A = A | mem[get_zpg_X()];
            set_NZ(A);
            pc += 2;
            break;
        case 6:
            //ORA abs,Y
            inst_cycles += 4;
            A = A | mem[get_abs_Y()];
            set_NZ(A);
            pc += 3;
            break;
        case 7:
            //ORA abs,X
            inst_cycles += 4;
            A = A | mem[get_abs_X()];
            set_NZ(A);
            pc += 3;
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
            break;
        case 1:
            //STA zpg
            break;
        case 3:
            //STA abs
            break;
        case 4:
            //STA ind,Y
            break;
        case 5:
            //STA zpg,X
            break;
        case 6:
            //STA abs,Y
            break;
        case 7:
            //STA abs,X
            break;
    }
}
void LDA(int mode){
    switch(mode){
        case 0:
            //LDA X,ind
            break;
        case 1:
            //LDA zpg
            break;
        case 2:
            //LDA #
            break;
        case 3:
            //LDA abs
            break;
        case 4:
            //LDA ind,Y
            break;
        case 5:
            //LDA zpg,X
            break;
        case 6:
            //LDA abs,Y
            break;
        case 7:
            //LDA abs,X
            break;
    }
}
void CMP(int mode){
    switch(mode){
        case 0:
            //CMP X,ind
            break;
        case 1:
            //CMP zpg
            break;
        case 2:
            //CMP #
            break;
        case 3:
            //CMP abs
            break;
        case 4:
            //CMP ind,Y
            break;
        case 5:
            //CMP zpg,X
            break;
        case 6:
            //CMP abs,Y
            break;
        case 7:
            //CMP abs,X
            break;
    }
}
void SBC(int mode){
    switch(mode){
        case 0:
            //SBC X,ind
            break;
        case 1:
            //SBC zpg
            break;
        case 2:
            //SBC #
            break;
        case 3:
            //SBC abs
            break;
        case 4:
            //SBC ind,Y
            break;
        case 5:
            //SBC zpg,X
            break;
        case 6:
            //SBC abs,Y
            break;
        case 7:
            //SBC abs,X
            break;
    }
}

////////////////////////////aaabbb10 Instructions
void ASL(int mode){
    switch(mode){
        case 1:
            //ASL zpg
            inst_cycles += 5;
            C = (mem[get_zpg()] >> 7) & 1;
            mem[get_zpg()] = mem[get_zpg()] << 1;
            set_NZ(mem[get_zpg()]);
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
            C = (mem[get_abs()] >> 7) & 1;
            mem[get_abs()] = mem[get_abs()] << 1;
            set_NZ(mem[get_abs()]);
            pc += 3;
            break;
        case 5:
            //ASL zpg,X
            inst_cycles += 6;
            C = (mem[get_zpg_X()] >> 7) & 1;
            mem[get_zpg_X()] = mem[get_zpg_X()] << 1;
            set_NZ(mem[get_zpg_X()]);
            pc += 2;
            break;
        case 7:
            //ASL abs,X
            inst_cycles += 7;
            C = (mem[get_abs_X()] >> 7) & 1;
            mem[get_abs_X()] = mem[get_abs_X()] << 1;
            set_NZ(mem[get_abs_X()]);
            pc += 3;
            break;
    }
}
void ROL(int mode){
    switch(mode){
        case 1:
            //ROL zpg
            inst_cycles += 5;
            C = (mem[get_zpg()] >> 7) & 1;
            mem[get_zpg()] = (mem[get_zpg()] << 1) + C;
            set_NZ(mem[get_zpg()]);
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
            C = (mem[get_abs()] >> 7) & 1;
            mem[get_abs()] = (mem[get_abs()] << 1) + C;
            set_NZ(mem[get_abs()]);
            pc += 3;
            break;
        case 5:
            //ROL zpg,X
            inst_cycles += 6;
            C = (mem[get_zpg_X()] >> 7) & 1;
            mem[get_zpg_X()] = (mem[get_zpg_X()] << 1) + C;
            set_NZ(mem[get_zpg_X()]);
            pc += 2;
            break;
        case 7:
            //ROL abs,X
            inst_cycles += 7;
            C = (mem[get_abs_X()] >> 7) & 1;
            mem[get_abs_X()] = (mem[get_abs_X()] << 1) + C;
            set_NZ(mem[get_abs_X()]);
            pc += 3;
            break;
    }
}
void LSR(int mode){
    switch(mode){
        case 1:
            //LSR zpg
            inst_cycles += 5;
            C = mem[get_zpg()] & 1;
            mem[get_zpg()] = mem[get_zpg()] >> 1;
            set_NZ(mem[get_zpg()]);
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
            C = mem[get_abs()] & 1;
            mem[get_abs()] = mem[get_abs()] >> 1;
            set_NZ(mem[get_abs()]);
            pc += 3;
            break;
        case 5:
            //LSR zpg,X
            inst_cycles += 6;
            C = mem[get_zpg_X()] & 1;
            mem[get_zpg_X()] = mem[get_zpg_X()] >> 1;
            set_NZ(mem[get_zpg_X()]);
            pc += 2;
            break;
        case 7:
            //LSR abs,X
            inst_cycles += 7;
            C = mem[get_abs_X()] & 1;
            mem[get_abs_X()] = mem[get_abs_X()] >> 1;
            set_NZ(mem[get_abs_X()]);
            pc += 3;
            break;
    }
}
void ROR(int mode){
    switch(mode){
        case 1:
            //ROR zpg
            inst_cycles += 5;
            C = mem[get_zpg()] & 1;
            mem[get_zpg()] = (mem[get_zpg()] >> 1) + (C << 7);
            set_NZ(mem[get_zpg()]);
            pc += 2;
            break;
        case 2:
            //ROR A
            inst_cycles += 2;
            C = A & 1;
            A = (A >> 1) + (C << 7);
            set_NZ(A);
            pc += 1;
            break;
        case 3:
            //ROR abs
            inst_cycles += 6;
            C = mem[get_abs()] & 1;
            mem[get_abs()] = (mem[get_abs()] >> 1) + (C << 7);
            set_NZ(mem[get_abs()]);
            pc += 3;
            break;
        case 5:
            //ROR zpg,X
            inst_cycles += 6;
            C = mem[get_zpg_X()] & 1;
            mem[get_zpg_X()] = (mem[get_zpg_X()] >> 1) + (C << 7);
            set_NZ(mem[get_zpg_X()]);
            pc += 2;
            break;
        case 7:
            //ROR abs,X
            inst_cycles += 7;
            C = mem[get_abs_X()] & 1;
            mem[get_abs_X()] = (mem[get_abs_X()] >> 1) + (C << 7);
            set_NZ(mem[get_abs_X()]);
            pc += 3;
            break;
    }
}
void STX(int mode){
    switch(mode){
        case 1:
            //STX zpg
            inst_cycles += 3;
            mem[get_zpg()] = X;
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
            mem[get_abs()] = X;
            pc += 3;
            break;
        case 5:
            //STX zpg,Y
            inst_cycles += 4;
            mem[get_zpg_Y()] = X;
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
            X = mem[get_zpg()];
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
            X = mem[get_abs()];
            set_NZ(X);
            pc += 3; 
            break;
        case 5:
            //LDX zpg,Y
            inst_cycles += 4;
            X = mem[get_zpg_Y()];
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
            X = mem[get_abs_Y()];
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
            mem[get_zpg()] = mem[get_zpg()] - 1;
            set_NZ(mem[get_zpg()]);
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
            mem[get_abs()] = mem[get_abs()] - 1;
            set_NZ(mem[get_abs()]);
            pc += 3;
            break;
        case 5:
            //DEC zpg,X
            inst_cycles += 6;
            mem[get_zpg_X()] = mem[get_zpg_X()] - 1;
            set_NZ(mem[get_zpg_X()]);
            pc += 2;
            break;
        case 7:
            //DEC abs,X
            inst_cycles += 7;
            mem[get_abs_X()] = mem[get_abs_X()] - 1;
            set_NZ(mem[get_abs_X()]);
            pc += 3;
            break;
    }
}
void INC(int mode){
    switch(mode){
        case 1:
            //INC zpg
            inst_cycles += 5;
            mem[get_zpg()] = mem[get_zpg()] + 1;
            set_NZ(mem[get_zpg()]);
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
            mem[get_abs()] = mem[get_abs()] + 1;
            set_NZ(mem[get_abs()]);
            pc += 3;
            break;
        case 5:
            //INC zpg,X
            inst_cycles += 6;
            mem[get_zpg_X()] = mem[get_zpg_X()] + 1;
            set_NZ(mem[get_zpg_X()]);
            pc += 2;
            break;
        case 7:
            //INC abs,X
            inst_cycles += 7;
            mem[get_abs_X()] = mem[get_abs_X()] + 1;
            set_NZ(mem[get_abs_X()]);
            pc += 3;
            break;
    }
}

/////////////////////////////Execute program
int inst_cycles;
int run(){
    while(true){
        //Get instruction
        char inst = get_from_memory(pc);
        char inst_a = inst >> 5;
        char inst_b = (inst >> 2) & 0b111;
        char inst_c = inst & 0b11;

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

        //TODO check for interrupts (take into account extra cycle special case)
    }
    return 0;
}

int main(int argc, char *argv[]) {

    //Load ROM
    //TODO

    //Execute
    return run();
}
