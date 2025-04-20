#include <cstdint>

/////////////////////////////Clock
uint64_t clock_cycle = 0;

/////////////////////////////Memory
//TODO

/////////////////////////////Registers/Flags
uint16_t pc;
uint8_t rA;
uint8_t rX;
uint8_t rY;
uint8_t rSP;
uint8_t rSR;
#define fN rSR[0]
#define fV rSR[1]
#define fB rSR[3]
#define fD rSR[4]
#define fI rSR[5]
#define fZ rSR[6]
#define fC rSR[7]

/////////////////////////////Addressing 
char get_from_mem(uint16_t addr){
    //read other values for instructions
}
uint16_t get_imm(){
    //immediate
}
uint16_t get_rel(){
    //relative
}
uint16_t get_zpg(){
    //zero page
}
uint16_t get_zpg_X(){
    //zero page X index
}
uint16_t get_zpg_Y(){
    //zero page Y index
}
uint16_t get_X_ind(){
    //indirect X pre-index
}
uint16_t get_ind_Y(){
    //indirect Y post-index
}
uint16_t get_abs(){
    //absolute
}
uint16_t get_abs_X(){
    //absolute X index
}
uint16_t get_abs_Y(){
    //absolute Y index
}

////////////////////////////aaabbb00 Instructions
void BRK(int mode){
    switch(mode){
        case 0:
            //BRK impl
            break;
        case 2:
            //PHP impl
            break;
        case 4:
            //BPL rel
            break;
        case 6:
            //CLC impl
            break;
    }
}
void JSR(int mode){
    switch(mode){
        case 0:
            //JSR abs
            break;
        case 1:
            //BIT zpg
            break;
        case 2:
            //PLP impl
            break;
        case 3:
            //BIT abs
            break;
        case 4:
            //BMI rel
            break;
        case 6:
            //SEC impl
            break;
    }
}
void RTI(int mode){
    switch(mode){
        case 0:
            //RTI impl
            break;
        case 2:
            //PHA impl
            break;
        case 3:
            //JMP abs
            break;
        case 4:
            //BVC rel
            break;
        case 6:
            //CLI impl
            break;
    }
}
void RTS(int mode){
    switch(mode){
        case 0:
            //RTS impl
            break;
        case 2:
            //PLA impl
            break;
        case 3:
            //JMP ind
            break;
        case 4:
            //BVS rel
            break;
        case 6:
            //SEI impl
            break;
    }
}
void STY(int mode){
    switch(mode){
        case 1:
            //STY zpg
            break;
        case 2:
            //DEY impl
            break;
        case 3:
            //STY abs
            break;
        case 4:
            //BCC rel
            break;
        case 5:
            //STY zpg,X
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
std::uint16_t new_pc;
int inst_cycles;
int run(){

    char inst = 0x00;
    char inst_a = inst >> 5;
    char inst_b = (inst >> 2) & 0b111;
    char inst_c = inst & 0b11;
    while(true){
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
    }
    clock_cycle += inst_cycles;
    inst = get_from_memory(new_pc);
    inst_a = inst >> 5;
    inst_b = (inst >> 2) & 0b111;
    inst_c = inst & 0b11;
    return 0;
}

int main(int argc, char *argv[]) {

    //Load ROM
    //TODO

    //Execute
    return run();
}
