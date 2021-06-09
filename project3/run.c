/***************************************************************/
/*                                                             */
/*   MIPS-32 Instruction Level Simulator                       */
/*                                                             */
/*   CS311 KAIST                                               */
/*   run.c                                                     */
/*                                                             */
/***************************************************************/

#include <stdio.h>

#include "util.h"
#include "run.h"


/***************************************************************/
/*                                                             */
/* Procedure: get_inst_info                                    */
/*                                                             */
/* Purpose: Read insturction information                       */
/*                                                             */
/***************************************************************/
instruction* get_inst_info(uint32_t pc) { 
    return &INST_INFO[(pc - MEM_TEXT_START) >> 2];
}

char ctrl_id_branch(instruction instr) {
    switch (OPCODE(&instr)) {
        case OP_J:
        case OP_JAL:
        case OP_BEQ:
        case OP_BNE:
            return TRUE;
        case OP_R:
            return FUNC(&instr) == FT_JR ? TRUE : FALSE;
    }
    return FALSE;
}

instruction parse_instr(uint32_t op) {
    instruction instr;
	/** Implement this function */
    instr.opcode = op >> 26;
    switch(instr.opcode)
    {
        case 0x2:
        case 0x3:
            // J type
            instr.r_t.target = op & 0x3ffffff;
            break;
        case 0x0:
            // R type
            instr.r_t.r_i.rs = (op >> 21) & 31;
            instr.r_t.r_i.rt = (op >> 16) & 31;
            instr.r_t.r_i.r_i.r.rd = (op >> 11) & 31;
            instr.r_t.r_i.r_i.r.shamt = (op >> 6) & 31;
            instr.func_code = op & 63;
            break;
        default:
            // I type
            instr.r_t.r_i.rs = (op >> 21) & 31;
            instr.r_t.r_i.rt = (op >> 16) & 31;
            instr.r_t.r_i.r_i.imm = op & 0xffff;
            break;
    }
    return instr;
}

Control_EX ctrl_ex(instruction instr) {
    Control_EX result = {0, 0};
    if (instr.value == 0) {
        return result;
    }
    result.alu_src = (OPCODE(&instr) == OP_BNE || OPCODE(&instr) == OP_BEQ || OPCODE(&instr) == OP_R) ? 0 : 1;
    result.reg_dst = OPCODE(&instr) == OP_R ? 1 : 0;
    return result;
}

Control_MEM ctrl_mem(instruction instr) {
    Control_MEM result = {0, 0};
    if (instr.value == 0) {
        return result;
    }
    result.mem_write = OPCODE(&instr) == OP_SW ? 1 : 0;
    result.mem_read = OPCODE(&instr) == OP_LW ? 1 : 0;
    result.branch = (OPCODE(&instr) == OP_BEQ || OPCODE(&instr) == OP_BNE) ? 1 : 0;
    return result;
}

Control_WB ctrl_wb(instruction instr) {
    Control_WB result = {0, 0};
    if (instr.value == 0) {
        return result;
    }
    // 0 MEM 1 ALU
    result.data_mux = OPCODE(&instr) == OP_LW ? 0 : 1;
    switch (OPCODE(&instr)) {
        case 0x4:
        case 0x5:
        case 0x2:
        case 0x3:
        case 0x2b:
            result.reg_write = 0;
            break;
        case 0x0:
            result.reg_write = FUNC(&instr) == 0x8 ? 0 : 1;
            break;
        default:
            result.reg_write = 1;
            break;
    }
    // printf("CTRL_WB %llx -> %llx\n", OPCODE(&instr), result.reg_write);
    return result;
}

uint32_t wb_result() {
    return CURRENT_STATE.MEM_WB_CTRL.data_mux ? CURRENT_STATE.MEM_WB_ALU_OUT : CURRENT_STATE.MEM_WB_MEM_OUT;
}

uint32_t process_alu(uint32_t op1, uint32_t op2, instruction* instr) {
    // op1 : rs, op2 : rt
    uint32_t shamt = SHAMT(instr);
    switch (OPCODE(instr)) {
        case 0x9:
        case 0x23:
        case 0x2b:
            return op1 + op2;
        case 0xc:
            return op1 & (0xffff & op2);
        case OP_BNE:
        case OP_BEQ:
            return op1 - op2;
        case 0xd:
            return op1 | op2;
        case 0xb:
            return (op1 < op2) ? 1 : 0;
        case 0xf:
            return op2 << 16;
    }
    switch(FUNC(instr)) {
        case 0x8:
            return op1;
        case 0x21:
            return op1 + op2;
        case 0x24:
            return op1 & op2;
        case 0x27:
            return ~(op1 | op2);
        case 0x25:
            return op1 | op2;
        case 0x2b:
            return (op1 < op2) ? 1 : 0;
        case 0x0:
            return op2 << SHAMT(instr);
        case 0x2:
            return op2 >> SHAMT(instr);
        case 0x23:
            return op1 - op2;
    }
    return 0;
}

void forward() {
    // EX forwarding
    CURRENT_STATE.FW_EX_OP1 = CURRENT_STATE.ID_EX_REG1;
    CURRENT_STATE.FW_EX_OP2 = CURRENT_STATE.ID_EX_REG2;
    char forward_1 = 0, forward_2 = 0;
    if (CURRENT_STATE.EX_MEM_CTRL_WB.reg_write) {
        uint32_t op = OPCODE(&CURRENT_STATE.EX_MEM_INSTR);
        if (op == 0) {
            if (RD(&CURRENT_STATE.EX_MEM_INSTR) != 0 &&
                RD(&CURRENT_STATE.EX_MEM_INSTR) == RS(&CURRENT_STATE.ID_EX_INSTR)) {
                #ifdef DEBUG
                printf("Foward OP1 EX_MEM RD->RS\n");
                #endif
                forward_1 = TRUE;
                CURRENT_STATE.FW_EX_OP1 = CURRENT_STATE.EX_MEM_ALU_OUT;
            }
            if (RD(&CURRENT_STATE.EX_MEM_INSTR) != 0 &&
                RD(&CURRENT_STATE.EX_MEM_INSTR) == RT(&CURRENT_STATE.ID_EX_INSTR)) {
                #ifdef DEBUG
                printf("Foward OP2 EX_MEM RD->RT\n");
                #endif
                forward_2 = TRUE;
                CURRENT_STATE.FW_EX_OP2 = CURRENT_STATE.EX_MEM_ALU_OUT;
            }
        } else if (op == OP_ADDIU || op == OP_ANDI || op == OP_LUI || op == OP_ORI || op == OP_SLTIU) {
            if (RT(&CURRENT_STATE.EX_MEM_INSTR) != 0 &&
                RT(&CURRENT_STATE.EX_MEM_INSTR) == RS(&CURRENT_STATE.ID_EX_INSTR)) {
                #ifdef DEBUG
                printf("Foward OP1 EX_MEM RT->RS\n");
                #endif
                forward_1 = TRUE;
                CURRENT_STATE.FW_EX_OP1 = CURRENT_STATE.EX_MEM_ALU_OUT;
            }
            if (RT(&CURRENT_STATE.EX_MEM_INSTR) != 0 &&
                RT(&CURRENT_STATE.EX_MEM_INSTR) == RT(&CURRENT_STATE.ID_EX_INSTR)) {
                #ifdef DEBUG
                printf("Foward OP2 EX_MEM RT->RT\n");
                #endif
                forward_2 = TRUE;
                CURRENT_STATE.FW_EX_OP2 = CURRENT_STATE.EX_MEM_ALU_OUT;
            }
        }
    }

    if (CURRENT_STATE.MEM_WB_CTRL.reg_write) {
        if (OPCODE(&CURRENT_STATE.MEM_WB_INSTR) == 0) {
            if (forward_1 == FALSE &&
                RD(&CURRENT_STATE.MEM_WB_INSTR) != 0 &&
                RD(&CURRENT_STATE.MEM_WB_INSTR) == RS(&CURRENT_STATE.ID_EX_INSTR)) {
                #ifdef DEBUG
                printf("Foward OP1 MEM_WB RD->RS\n");
                #endif
                CURRENT_STATE.FW_EX_OP1 = wb_result();
            }
            if (forward_2 == FALSE &&
                RD(&CURRENT_STATE.MEM_WB_INSTR) != 0 &&
                RD(&CURRENT_STATE.MEM_WB_INSTR) == RT(&CURRENT_STATE.ID_EX_INSTR)) {
                #ifdef DEBUG
                printf("Foward OP2 MEM_WB RD->RT\n");
                #endif
                CURRENT_STATE.FW_EX_OP2 = wb_result();
            }
        } else {
            if (forward_1 == FALSE &&
                RT(&CURRENT_STATE.MEM_WB_INSTR) != 0 &&
                RT(&CURRENT_STATE.MEM_WB_INSTR) == RS(&CURRENT_STATE.ID_EX_INSTR)) {
                #ifdef DEBUG
                printf("Foward OP1 MEM_WB RT->RS\n");
                #endif
                CURRENT_STATE.FW_EX_OP1 = wb_result();
            }
            if (forward_2 == FALSE &&
                RT(&CURRENT_STATE.MEM_WB_INSTR) != 0 &&
                RT(&CURRENT_STATE.MEM_WB_INSTR) == RT(&CURRENT_STATE.ID_EX_INSTR)) {
                #ifdef DEBUG
                printf("Foward OP2 MEM_WB RT->RT\n");
                #endif
                CURRENT_STATE.FW_EX_OP2 = wb_result();
            }
        }
    }
    
    // MEM forwarding
    CURRENT_STATE.FW_MEM_DATA = CURRENT_STATE.EX_MEM_W_VALUE;
    if (OPCODE(&CURRENT_STATE.MEM_WB_INSTR) == 0x23 &&  // MEM-WB lw
        OPCODE(&CURRENT_STATE.EX_MEM_INSTR) == 0x2b &&  // EX-MEM sw
        RT(&CURRENT_STATE.MEM_WB_INSTR) == RT(&CURRENT_STATE.EX_MEM_INSTR)) {
        CURRENT_STATE.FW_MEM_DATA = wb_result();
    }
    // ID branch forwarding
    CURRENT_STATE.IF_ID_CMP_1 = 0;
    CURRENT_STATE.IF_ID_CMP_2 = 0;
    if (ctrl_id_branch(CURRENT_STATE.ID_EX_INSTR)) {
        if (RD(&CURRENT_STATE.EX_MEM_INSTR) != 0 &&
            RD(&CURRENT_STATE.EX_MEM_INSTR) == RS(&CURRENT_STATE.IF_ID_INSTR)) {
            CURRENT_STATE.IF_ID_CMP_1 = 1;
        }
        if (RD(&CURRENT_STATE.EX_MEM_INSTR) != 0 &&
            RD(&CURRENT_STATE.EX_MEM_INSTR) == RT(&CURRENT_STATE.IF_ID_INSTR)) {
            CURRENT_STATE.IF_ID_CMP_2 = 1;
        }
    }
}

void process_writeback() {
    if (CURRENT_STATE.MEM_WB_NPC != 0) {
        INSTRUCTION_COUNT++;
    }
    CURRENT_STATE.PIPE[WB_STAGE] = CURRENT_STATE.MEM_WB_NPC;
    #ifdef DEBUG
    printf(" >     WB_OP: %llx\n", CURRENT_STATE.MEM_WB_INSTR.opcode);
    printf(" >  WB_WRITE: %d\n", CURRENT_STATE.MEM_WB_CTRL.reg_write);
    printf(" >    WB_REG: %d\n", CURRENT_STATE.MEM_WB_WB_REG);
    printf(" >    WB_MUX: %d\n", CURRENT_STATE.MEM_WB_CTRL.data_mux);
    printf(" >   WB_DATA: %llx\n", wb_result());
    #endif
    if (!CURRENT_STATE.MEM_WB_CTRL.reg_write) {
        return;
    }
    CURRENT_STATE.REGS[CURRENT_STATE.MEM_WB_WB_REG] = wb_result();
}

void process_memory() {
    CURRENT_STATE.PIPE[MEM_STAGE] = CURRENT_STATE.EX_MEM_NPC;
    if (CURRENT_STATE.EX_MEM_CTRL.mem_read) {
        CURRENT_STATE.MEM_WB_MEM_OUT = mem_read_32(CURRENT_STATE.EX_MEM_ALU_OUT);
    } else {
        CURRENT_STATE.MEM_WB_MEM_OUT = 0x0;
    }
    #ifdef DEBUG
    printf(" >    MEM_OP: %llx\n", CURRENT_STATE.EX_MEM_INSTR.opcode);
    printf(" >  MEM_ADDR: %llx\n", CURRENT_STATE.EX_MEM_ALU_OUT);
    printf(" >  MEM_DATA: %llx\n", CURRENT_STATE.FW_MEM_DATA);
    printf(" >   MEM_OUT: %llx\n", CURRENT_STATE.MEM_WB_MEM_OUT);
    printf(" >  MEM_READ: %d\n", CURRENT_STATE.EX_MEM_CTRL.reg_read);
    printf(" > MEM_WRITE: %d\n", CURRENT_STATE.EX_MEM_CTRL.reg_write);
    printf(" >  MEM_TAKE: %llu\n", CURRENT_STATE.EX_MEM_BR_TAKE);
    printf(" >   MEM_TGT: %llx\n", CURRENT_STATE.EX_MEM_BR_TARGET);
    #endif
    if (CURRENT_STATE.EX_MEM_CTRL.mem_write) {
        mem_write_32(CURRENT_STATE.EX_MEM_ALU_OUT, CURRENT_STATE.FW_MEM_DATA);
    }

    unsigned char take = CURRENT_STATE.EX_MEM_CTRL.branch & CURRENT_STATE.EX_MEM_BR_TAKE;
    if (take) {
        #ifdef DEBUG
        printf("MEM taken\n");
        #endif
        CURRENT_STATE.BRANCH_PC = CURRENT_STATE.EX_MEM_NPC + 4 + CURRENT_STATE.EX_MEM_BR_TARGET;
        CURRENT_STATE.PC_MUX_BRANCH = 1;
        CURRENT_STATE.PC_MUX_JMP = 0;
        CURRENT_STATE.IF_STALL = TRUE;
        CURRENT_STATE.ID_STALL = TRUE;
        CURRENT_STATE.EX_STALL = TRUE;
    }

    // Pass-through values
    CURRENT_STATE.MEM_WB_INSTR = CURRENT_STATE.EX_MEM_INSTR;
    CURRENT_STATE.MEM_WB_NPC = CURRENT_STATE.EX_MEM_NPC;
    CURRENT_STATE.MEM_WB_ALU_OUT = CURRENT_STATE.EX_MEM_ALU_OUT;
    CURRENT_STATE.MEM_WB_WB_REG = CURRENT_STATE.EX_MEM_WB_REG;
    CURRENT_STATE.MEM_WB_CTRL = CURRENT_STATE.EX_MEM_CTRL_WB;
}

void process_execute() {
    if (CURRENT_STATE.EX_STALL) {
        #ifdef DEBUG
        printf("EX STALL!!\n");
        #endif
        CURRENT_STATE.PIPE[EX_STAGE] = 0x0;
        CURRENT_STATE.EX_MEM_INSTR = parse_instr(0x0);
        CURRENT_STATE.EX_MEM_CTRL = ctrl_mem(CURRENT_STATE.EX_MEM_INSTR);
        CURRENT_STATE.EX_MEM_CTRL_WB = ctrl_wb(CURRENT_STATE.EX_MEM_INSTR);
        CURRENT_STATE.EX_MEM_NPC = 0x0;
        return;
    }
    CURRENT_STATE.PIPE[EX_STAGE] = CURRENT_STATE.ID_EX_NPC;
    uint32_t op1 = CURRENT_STATE.FW_EX_OP1;
    uint32_t op2 = CURRENT_STATE.ID_EX_CTRL.alu_src ? CURRENT_STATE.ID_EX_IMM : CURRENT_STATE.FW_EX_OP2;
    #ifdef DEBUG
    printf(" >     EX_IR: %llx\n", CURRENT_STATE.ID_EX_INSTR.value);
    printf(" >     EX_OP: %llx\n", OPCODE(&CURRENT_STATE.ID_EX_INSTR));
    printf(" >   EX_FUNC: %llx\n", CURRENT_STATE.ID_EX_INSTR.func_code);
    printf(" >    EX_OP1: %llx\n", op1);
    printf(" >    EX_OP2: %llx\n", op2);
    printf(" >   reg_dst: %d\n", CURRENT_STATE.ID_EX_CTRL.reg_dst);
    #endif
    CURRENT_STATE.EX_MEM_ALU_OUT = process_alu(op1, op2, &CURRENT_STATE.ID_EX_INSTR);
    CURRENT_STATE.EX_MEM_W_VALUE = CURRENT_STATE.ID_EX_REG2;
    CURRENT_STATE.EX_MEM_WB_REG = CURRENT_STATE.ID_EX_CTRL.reg_dst ? RD(&CURRENT_STATE.ID_EX_INSTR) : RT(&CURRENT_STATE.ID_EX_INSTR);
    CURRENT_STATE.EX_MEM_BR_TARGET = CURRENT_STATE.ID_EX_IMM << 2;
    CURRENT_STATE.EX_MEM_BR_TAKE = FALSE;
    #ifdef DEBUG
    printf(" >    EX_OUT: %llx\n", CURRENT_STATE.EX_MEM_ALU_OUT);
    #endif
    if (OPCODE(&CURRENT_STATE.ID_EX_INSTR) == OP_BEQ) {
        CURRENT_STATE.EX_MEM_BR_TAKE = CURRENT_STATE.EX_MEM_ALU_OUT == 0 ? TRUE : FALSE;
        #ifdef DEBUG
        printf(" >  BEQ TAKE: %d\n", CURRENT_STATE.EX_MEM_BR_TAKE);
        #endif
    }
    else if (OPCODE(&CURRENT_STATE.ID_EX_INSTR) == OP_BNE) {
        CURRENT_STATE.EX_MEM_BR_TAKE = CURRENT_STATE.EX_MEM_ALU_OUT != 0 ? TRUE : FALSE;
        #ifdef DEBUG
        printf(" >  BNE TAKE: %d\n", CURRENT_STATE.EX_MEM_BR_TAKE);
        #endif
    }

    // Pass-through values
    CURRENT_STATE.EX_MEM_INSTR = CURRENT_STATE.ID_EX_INSTR;
    CURRENT_STATE.EX_MEM_NPC = CURRENT_STATE.ID_EX_NPC;
    CURRENT_STATE.EX_MEM_CTRL = CURRENT_STATE.ID_EX_CTRL_MEM;
    CURRENT_STATE.EX_MEM_CTRL_WB = CURRENT_STATE.ID_EX_CTRL_WB;
}

void process_decode() {
    if (CURRENT_STATE.ID_STALL) {
        #ifdef DEBUG
        printf("ID STALL!!\n");
        #endif
        CURRENT_STATE.PIPE[ID_STAGE] = 0x0;
        CURRENT_STATE.ID_EX_INSTR = parse_instr(0x0);
        CURRENT_STATE.ID_EX_CTRL = ctrl_ex(CURRENT_STATE.ID_EX_INSTR);
        CURRENT_STATE.ID_EX_CTRL_MEM = ctrl_mem(CURRENT_STATE.ID_EX_INSTR);
        CURRENT_STATE.ID_EX_CTRL_WB = ctrl_wb(CURRENT_STATE.ID_EX_INSTR);
        CURRENT_STATE.ID_EX_NPC = 0x0;
        return;
    }
    CURRENT_STATE.PIPE[ID_STAGE] = CURRENT_STATE.IF_ID_NPC;
    #ifdef DEBUG
    printf(" >     ID_IR: %llx\n", CURRENT_STATE.IF_ID_INSTR.value);
    printf(" >     ID_PC: %llx\n", CURRENT_STATE.IF_ID_NPC);
    printf(" >     ID_OP: %llx\n", OPCODE(&CURRENT_STATE.IF_ID_INSTR));
    #endif

    // Jumps
    if (OPCODE(&CURRENT_STATE.IF_ID_INSTR) == OP_J || OPCODE(&CURRENT_STATE.IF_ID_INSTR) == OP_JAL) {
        if (OPCODE(&CURRENT_STATE.IF_ID_INSTR) == OP_JAL) {
            CURRENT_STATE.REGS[31] = CURRENT_STATE.IF_ID_NPC + 4;
        }
        CURRENT_STATE.IF_ID_FLUSH = TRUE;
        CURRENT_STATE.JUMP_PC = CURRENT_STATE.IF_ID_INSTR.r_t.target << 2;
        CURRENT_STATE.PC_MUX_JMP = 1;
    }
    if (OPCODE(&CURRENT_STATE.IF_ID_INSTR) == OP_R && FUNC(&CURRENT_STATE.IF_ID_INSTR) == FT_JR) {
        CURRENT_STATE.IF_ID_FLUSH = TRUE;
        CURRENT_STATE.JUMP_PC = CURRENT_STATE.REGS[RS(&CURRENT_STATE.IF_ID_INSTR)];
        CURRENT_STATE.PC_MUX_JMP = 1;
    }

    // Hazard detection
    if (CURRENT_STATE.ID_EX_CTRL_MEM.mem_read == TRUE &&
        (RT(&CURRENT_STATE.ID_EX_INSTR) == RS(&CURRENT_STATE.IF_ID_INSTR) || 
         RT(&CURRENT_STATE.ID_EX_INSTR) == RT(&CURRENT_STATE.IF_ID_INSTR))) {
        // Stall the pipeline
        #ifdef DEBUG
        printf("Hazard detected!\n");
        #endif
        CURRENT_STATE.T_PC_WRITE = FALSE;
        CURRENT_STATE.IF_ID_WRITE = FALSE;
        CURRENT_STATE.ID_EX_INSTR = parse_instr(0x0);
        CURRENT_STATE.ID_EX_CTRL = ctrl_ex(CURRENT_STATE.ID_EX_INSTR);
        CURRENT_STATE.ID_EX_CTRL_MEM = ctrl_mem(CURRENT_STATE.ID_EX_INSTR);
        CURRENT_STATE.ID_EX_CTRL_WB = ctrl_wb(CURRENT_STATE.ID_EX_INSTR);
        CURRENT_STATE.ID_EX_NPC = 0x0;
        CURRENT_STATE.ID_EX_IMM = 0x0;
        return;
    }

    CURRENT_STATE.ID_EX_CTRL = ctrl_ex(CURRENT_STATE.IF_ID_INSTR);
    CURRENT_STATE.ID_EX_CTRL_MEM = ctrl_mem(CURRENT_STATE.IF_ID_INSTR);
    CURRENT_STATE.ID_EX_CTRL_WB = ctrl_wb(CURRENT_STATE.IF_ID_INSTR);
    CURRENT_STATE.ID_EX_REG1 = CURRENT_STATE.REGS[RS(&CURRENT_STATE.IF_ID_INSTR)];
    CURRENT_STATE.ID_EX_REG2 = CURRENT_STATE.REGS[RT(&CURRENT_STATE.IF_ID_INSTR)];

    // Pass-through values
    CURRENT_STATE.ID_EX_INSTR = CURRENT_STATE.IF_ID_INSTR;
    CURRENT_STATE.ID_EX_NPC = CURRENT_STATE.IF_ID_NPC;
    CURRENT_STATE.ID_EX_IMM = SIGN_EX(IMM(&CURRENT_STATE.IF_ID_INSTR));

    // Branch operations
    // CURRENT_STATE.IF_ID_FLUSH = 0;
    // if (ctrl_id_branch(CURRENT_STATE.IF_ID_INSTR)) {
    //     char mux1 = CURRENT_STATE.IF_ID_CMP_1;
    //     char mux2 = CURRENT_STATE.IF_ID_CMP_2;
    //     uint32_t op1 = mux1 ? CURRENT_STATE.EX_MEM_ALU_OUT : CURRENT_STATE.ID_EX_REG1;
    //     uint32_t op1 = mux2 ? CURRENT_STATE.EX_MEM_ALU_OUT : CURRENT_STATE.ID_EX_REG2;
    // }
}

void process_fetch() {
    if (CURRENT_STATE.IF_STALL) {
        #ifdef DEBUG
        printf("IF STALL!!\n");
        #endif
        CURRENT_STATE.PIPE[IF_STAGE] = 0x0;
        CURRENT_STATE.IF_ID_INST = 0x0;
        CURRENT_STATE.IF_ID_NPC = 0x0;
        return;
    }
    CURRENT_STATE.PIPE[IF_STAGE] = CURRENT_STATE.PC;
    if (!CURRENT_STATE.IF_ID_WRITE) {
        return;
    }
    if (CURRENT_STATE.IF_ID_FLUSH) {
        CURRENT_STATE.IF_ID_INST = 0x0;
        CURRENT_STATE.IF_ID_NPC = 0x0;
    } else if (CURRENT_STATE.PC >= MEM_TEXT_START + (NUM_INST << 2)) {
        CURRENT_STATE.PIPE[IF_STAGE] = 0;
        CURRENT_STATE.IF_ID_INST = 0x0;
        CURRENT_STATE.IF_ID_NPC = 0x0;
    } else {
        CURRENT_STATE.IF_ID_INST = mem_read_32(CURRENT_STATE.PC);
        CURRENT_STATE.IF_ID_NPC = CURRENT_STATE.PC;
    }
    CURRENT_STATE.IF_PC = CURRENT_STATE.PC + 4;
}

unsigned char pipeline_is_empty() {
    return CURRENT_STATE.PIPE[0] + CURRENT_STATE.PIPE[1] + CURRENT_STATE.PIPE[2] + CURRENT_STATE.PIPE[3] == 0 ? TRUE : FALSE;
}

void process_pc() {
    #ifdef DEBUG
    printf(" > PC_M_BRCH: %d\n", CURRENT_STATE.PC_MUX_BRANCH);
    printf(" >  PC_M_JMP: %d\n", CURRENT_STATE.PC_MUX_JMP);
    #endif
    if (!CURRENT_STATE.PC_WRITE) {
        return;
    }
    uint32_t mux1 = CURRENT_STATE.PC_MUX_BRANCH == 1 ? CURRENT_STATE.BRANCH_PC : CURRENT_STATE.IF_PC;
    uint32_t mux2 = CURRENT_STATE.PC_MUX_JMP == 1 ? CURRENT_STATE.JUMP_PC : mux1;
    CURRENT_STATE.PC = mux2;
}


/***************************************************************/
/*                                                             */
/* Procedure: process_instruction                              */
/*                                                             */
/* Purpose: Process one instrction                             */
/*                                                             */
/***************************************************************/
void process_instruction(){
    #ifdef DEBUG
    printf("PC: %x\n", CURRENT_STATE.PC);
    #endif
    CURRENT_STATE.IF_ID_INSTR = parse_instr(CURRENT_STATE.IF_ID_INST);
    if (CURRENT_STATE.IF_ID_INST == 0) {
        CURRENT_STATE.IF_ID_INSTR.value = 0;
    }
    // Clear branch states
    CURRENT_STATE.IF_ID_WRITE = TRUE;
    CURRENT_STATE.PC_WRITE = 1;
    CURRENT_STATE.PC_MUX_BRANCH = 0;
    CURRENT_STATE.PC_MUX_JMP = 0;
    if (CURRENT_STATE.PC >= MEM_TEXT_START + (NUM_INST << 2)) {
        CURRENT_STATE.PC_WRITE = 0;
    }

    CURRENT_STATE.T_PC_WRITE = TRUE;
    CURRENT_STATE.T_IF_ID_WRITE = TRUE;
    CURRENT_STATE.IF_STALL = FALSE;
    CURRENT_STATE.ID_STALL = FALSE;
    CURRENT_STATE.EX_STALL = FALSE;
    CURRENT_STATE.IF_ID_FLUSH = FALSE;

	forward();
    process_writeback();
    process_memory();
    process_execute();
    process_decode();
    process_fetch();
    process_pc();

    #ifdef DEBUG
    printf("T_PC_WRITE %d\n", CURRENT_STATE.T_PC_WRITE);
    printf("T_IF_ID_WRITE %d\n", CURRENT_STATE.T_IF_ID_WRITE);
    #endif

    CURRENT_STATE.PC_WRITE = CURRENT_STATE.T_PC_WRITE;
    CURRENT_STATE.IF_ID_WRITE = CURRENT_STATE.T_IF_ID_WRITE;

    if (CURRENT_STATE.PC == MEM_TEXT_START + (NUM_INST << 2) && pipeline_is_empty()) {
        RUN_BIT = 0;
    }
}
