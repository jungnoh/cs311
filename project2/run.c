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
instruction* get_inst_info(uint32_t pc) 
{ 
    return &INST_INFO[(pc - MEM_TEXT_START) >> 2];
}

uint32_t signExtImm(instruction* inst) {
    return ((0xffffu * (0x1 & (inst->r_t.r_i.r_i.imm >> 15))) << 16) | inst->r_t.r_i.r_i.imm;
}

uint32_t zeroExtImm(instruction* inst) {
    return 0x0000ffff & inst->r_t.r_i.r_i.imm;
}

uint32_t branchAddr(instruction* inst) {
    return ((0x3fffu * (0x1 & (inst->r_t.r_i.r_i.imm >> 15))) << 16) | (inst->r_t.r_i.r_i.imm << 2);
}

/***************************************************************/
/*                                                             */
/* Procedure: process_instruction                              */
/*                                                             */
/* Purpose: Process one instrction                             */
/*                                                             */
/***************************************************************/
void process_instruction(){
    if (CURRENT_STATE.PC >= MEM_TEXT_START + NUM_INST*4) {
        RUN_BIT = FALSE;
        return;
    }
	/** Implement this function */
    instruction* inst = get_inst_info(CURRENT_STATE.PC);
    char is_r = FALSE;
    char add_pc = TRUE;
    switch (inst->opcode) {
        case 0x0:
            is_r = TRUE;
            break;
        case 0x2:
            CURRENT_STATE.PC = (CURRENT_STATE.PC & 0xf0000000) | (inst->r_t.target << 2);
            add_pc = FALSE;
            break;
        case 0x3:
            CURRENT_STATE.REGS[31] = CURRENT_STATE.PC + 4;
            CURRENT_STATE.PC = (CURRENT_STATE.PC & 0xf0000000) | (inst->r_t.target << 2);
            add_pc = FALSE;
            break;
        case 0x9:
            CURRENT_STATE.REGS[inst->r_t.r_i.rt] = CURRENT_STATE.REGS[inst->r_t.r_i.rs] + signExtImm(inst);
            break;
        case 0xc:
            CURRENT_STATE.REGS[inst->r_t.r_i.rt] = CURRENT_STATE.REGS[inst->r_t.r_i.rs] & zeroExtImm(inst);
            break;
        case 0x4:
            if (CURRENT_STATE.REGS[inst->r_t.r_i.rs] == CURRENT_STATE.REGS[inst->r_t.r_i.rt]) {
                CURRENT_STATE.PC += (4 + branchAddr(inst));
                add_pc = FALSE;
            }
            break;
        case 0x5:
            if (CURRENT_STATE.REGS[inst->r_t.r_i.rs] != CURRENT_STATE.REGS[inst->r_t.r_i.rt]) {
                CURRENT_STATE.PC += (4 + branchAddr(inst));
                add_pc = FALSE;
            }
            break;
        case 0xf:
            CURRENT_STATE.REGS[inst->r_t.r_i.rt] = ((uint32_t)inst->r_t.r_i.r_i.imm) << 16;
            break;
        case 0x23:
            CURRENT_STATE.REGS[inst->r_t.r_i.rt] = mem_read_32(CURRENT_STATE.REGS[inst->r_t.r_i.rs] + signExtImm(inst));
            break;
        case 0xd:
            CURRENT_STATE.REGS[inst->r_t.r_i.rt] = CURRENT_STATE.REGS[inst->r_t.r_i.rs] | zeroExtImm(inst);
            break;
        case 0xb:
            CURRENT_STATE.REGS[inst->r_t.r_i.rt] = CURRENT_STATE.REGS[inst->r_t.r_i.rs] < signExtImm(inst) ? 1 : 0;
            break;
        case 0x2b:
            mem_write_32(CURRENT_STATE.REGS[inst->r_t.r_i.rs] + signExtImm(inst), CURRENT_STATE.REGS[inst->r_t.r_i.rt]);
            break;
        default:
            is_r = TRUE;
            break;
    }
    if (!is_r) {
        if(add_pc) {
            CURRENT_STATE.PC += 4;
        }
        return;
    }
    switch(inst->func_code) {
        case 0x21:
            // printf("ADDU %x %x %x", CURRENT_STATE.REGS[inst->r_t.r_i.r_i.r.rd], CURRENT_STATE.REGS[inst->r_t.r_i.rs], CURRENT_STATE.REGS[inst->r_t.r_i.rt]);
            CURRENT_STATE.REGS[inst->r_t.r_i.r_i.r.rd] = CURRENT_STATE.REGS[inst->r_t.r_i.rs] + CURRENT_STATE.REGS[inst->r_t.r_i.rt];
            break;
        case 0x24:
            CURRENT_STATE.REGS[inst->r_t.r_i.r_i.r.rd] = CURRENT_STATE.REGS[inst->r_t.r_i.rs] & CURRENT_STATE.REGS[inst->r_t.r_i.rt];
            break;
        case 0x8:
            CURRENT_STATE.PC = CURRENT_STATE.REGS[inst->r_t.r_i.rs];
            add_pc = FALSE;
            break;
        case 0x27:
            CURRENT_STATE.REGS[inst->r_t.r_i.r_i.r.rd] = ~(CURRENT_STATE.REGS[inst->r_t.r_i.rs] | CURRENT_STATE.REGS[inst->r_t.r_i.rt]);
            break;
        case 0x25:
            CURRENT_STATE.REGS[inst->r_t.r_i.r_i.r.rd] = CURRENT_STATE.REGS[inst->r_t.r_i.rs] | CURRENT_STATE.REGS[inst->r_t.r_i.rt];
            break;
        case 0x2b:
            CURRENT_STATE.REGS[inst->r_t.r_i.r_i.r.rd] = (CURRENT_STATE.REGS[inst->r_t.r_i.rs] < CURRENT_STATE.REGS[inst->r_t.r_i.rt]) ? 1 : 0;
            break;
        case 0x0:
            CURRENT_STATE.REGS[inst->r_t.r_i.r_i.r.rd] = CURRENT_STATE.REGS[inst->r_t.r_i.rt] << inst->r_t.r_i.r_i.r.shamt;
            break;
        case 0x2:
            CURRENT_STATE.REGS[inst->r_t.r_i.r_i.r.rd] = CURRENT_STATE.REGS[inst->r_t.r_i.rt] >> inst->r_t.r_i.r_i.r.shamt;
            break;
        case 0x23:
            CURRENT_STATE.REGS[inst->r_t.r_i.r_i.r.rd] = CURRENT_STATE.REGS[inst->r_t.r_i.rs] - CURRENT_STATE.REGS[inst->r_t.r_i.rt];
            break;
    }
    if(add_pc) {
        CURRENT_STATE.PC += 4;
    }
}
