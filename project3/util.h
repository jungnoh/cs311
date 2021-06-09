/***************************************************************/
/*                                                             */
/*   MIPS-32 Instruction Level Simulator                       */
/*                                                             */
/*   CS311 KAIST                                               */
/*   util.h                                                    */
/*                                                             */
/***************************************************************/

#ifndef _UTIL_H_
#define _UTIL_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FALSE 0
#define TRUE  1

/* Basic Information */
#define MEM_TEXT_START	0x00400000
#define MEM_TEXT_SIZE	0x00100000
#define MEM_DATA_START	0x10000000
#define MEM_DATA_SIZE	0x00100000
#define MIPS_REGS	32
#define BYTES_PER_WORD	4
#define PIPE_STAGE	5

#define IF_STAGE 	0
#define ID_STAGE	1
#define EX_STAGE	2
#define MEM_STAGE	3
#define WB_STAGE	4

/* Instructions */
#define OP_LW  0x23
#define OP_SW  0x2b
#define OP_BEQ 0x4
#define OP_BNE 0x5
#define OP_R   0x0
#define OP_J   0x2
#define OP_JAL 0x3
#define FT_JR  0x8
#define OP_ADDIU 0x9
#define OP_ANDI 0xc
#define OP_LUI 0xf
#define OP_ORI 0xd
#define OP_SLTIU 0xb


typedef struct inst_s {
    short opcode;
    
    /*R-type*/
    short func_code;

    union {
        /* R-type or I-type: */
        struct {
	    unsigned char rs;
	    unsigned char rt;

	    union {
	        short imm;

	        struct {
		    unsigned char rd;
		    unsigned char shamt;
		} r;
	    } r_i;
	} r_i;
        /* J-type: */
        uint32_t target;
    } r_t;

    uint32_t value;
    
    //int32 encoding;
    //imm_expr *expr;
    //char *source_line;
} instruction;

typedef struct Control_WB_Struct {
	// Selection bit for register data.
	// 0 = MEM output, 1 = ALU output
	unsigned char data_mux;
	// Activation bit for register write
	unsigned char reg_write;
} Control_WB;

typedef struct Control_MEM_Struct {
	// Activation bit for memory write
	unsigned char mem_write;
	// Activation bit for memory read
	unsigned char mem_read;
	// Branch bit
	unsigned char branch;
} Control_MEM;

typedef struct Control_EX_Struct {
	// Selection bit for ALU op2
	// 0 = REG output (or forwarded value), 1 = Immediate field
	unsigned char alu_src;
	// Destination register (forwarded and used in WB stage)
	unsigned char reg_dst;
} Control_EX;

/* You may add pipeline registers that you require */
typedef struct CPU_State_Struct {
	// Program counter for the IF stage
	uint32_t PC;
	// Register file
	uint32_t REGS[MIPS_REGS];
	// PC being executed at each stage
	uint32_t PIPE[PIPE_STAGE];
	
	//===============================================================
	// IF/ID Latch
	//===============================================================
	uint32_t IF_ID_INST;          // Raw Instruction
	instruction IF_ID_INSTR;      // Instruction
	uint32_t IF_ID_NPC;           // PC for ID stage
	unsigned char IF_ID_FLUSH;    // Flush the next instruction
	unsigned char IF_ID_CMP_1;    // Mux 1 selection
	unsigned char IF_ID_CMP_2;    // Mux 2 selection

	//===============================================================
	// ID/EX Latch
	//===============================================================
	instruction ID_EX_INSTR;      // Instruction for EX stage
	uint32_t ID_EX_NPC;           // PC for EX stage
	uint32_t ID_EX_REG1;          // Register 1 value
	uint32_t ID_EX_REG2;          // Register 2 value
	short ID_EX_IMM;              // Sign extended immediate field
	Control_EX  ID_EX_CTRL;       // EX stage controls
	Control_MEM ID_EX_CTRL_MEM;   // MEM stage controls
	Control_WB  ID_EX_CTRL_WB;    // WB stage controls

	//===============================================================
	// EX/MEM Latch
	//===============================================================
	instruction EX_MEM_INSTR;     // Instruction for MEM stage
	uint32_t EX_MEM_NPC;          // PC for MEM stage
	uint32_t EX_MEM_ALU_OUT;      // ALU output
	uint32_t EX_MEM_W_VALUE;      // Write value (=ID_EX_REG2)
	uint32_t EX_MEM_BR_TARGET;    // Branch instruction target
	uint32_t EX_MEM_BR_TAKE;      // Indicating whether branch is taken
	uint32_t EX_MEM_WB_REG;       // WB register #
	Control_MEM EX_MEM_CTRL;      // MEM stage controls
	Control_WB  EX_MEM_CTRL_WB;   // WB stage controls

	//===============================================================
	// MEM/WB Latch
	//===============================================================
	instruction MEM_WB_INSTR;     // Instruction for WB stage
	uint32_t MEM_WB_NPC;          // PC for WB stage
	uint32_t MEM_WB_ALU_OUT;      // ALU output (DEST==1)
	uint32_t MEM_WB_MEM_OUT;      // MEM output (DEST==0)
	uint32_t MEM_WB_WB_REG;       // WB register #
	Control_WB  MEM_WB_CTRL;      // WB stage controls

	//===============================================================
	// States used during a cycle
	// These values are not to be used to pass data between cycles.
	//===============================================================

	// Forwarded result of ALU op1
	uint32_t FW_EX_OP1;
	// Forwarded result of ALU reg2 (connected to op2 with mux)
	uint32_t FW_EX_OP2;
	// Forwarded result of MEM write data input
	uint32_t FW_MEM_DATA;

	// PC selection bit for branch mux
	char PC_MUX_BRANCH;
	// PC selection bit for jump mux
	char PC_MUX_JMP;
	// PC register activation bit
	char PC_WRITE;

	// PC proposed by IF stage (PC<-PC+4)
	uint32_t IF_PC;
	// PC proposed by Jump instruction (absolute)
	uint32_t JUMP_PC;
	// PC proposed by Branch instruction (absolute)
	uint32_t BRANCH_PC;

	//===============================================================
	// Additional control bits
	//===============================================================

	// Stall the IF stage (ie. empty instruction, PC, controls)
	unsigned char IF_STALL;
	// Stall the ID stage (ie. empty instruction, PC, controls)
	unsigned char ID_STALL;
	// Stall the EX stage (ie. empty instruction, PC, controls)
	unsigned char EX_STALL;

	// Control bit to activate the IF/ID stage latch
	unsigned char IF_ID_WRITE;
	// IF_ID_WRITE value to be used in the NEXT STAGE
	unsigned char T_IF_ID_WRITE;
	// PC_WRITE value to be used in the NEXT STAGE
	unsigned char T_PC_WRITE;
} CPU_State;


typedef struct {
    uint32_t start, size;
    uint8_t *mem;
} mem_region_t;

/* For PC * Registers */
extern CPU_State CURRENT_STATE;

/* For Instructions */
extern instruction *INST_INFO;
extern int NUM_INST;

/* For Memory Regions */
extern mem_region_t MEM_REGIONS[2];

/* For Execution */
extern int RUN_BIT;	/* run bit */
extern int FETCH_BIT;	/* instruction fetch bit */
extern int INSTRUCTION_COUNT;

extern int BR_BIT;	/* Branch predictor enabled */
extern int FORWARDING_BIT;
extern uint64_t MAX_INSTRUCTION_NUM;
extern uint64_t CYCLE_COUNT;

/* Functions */
char**		str_split(char *a_str, const char a_delim);
int		fromBinary(const char *s);
uint32_t	mem_read_32(uint32_t address);
void		mem_write_32(uint32_t address, uint32_t value);
void		cycle();
void		run();
void		go();
void		mdump(int start, int stop);
void		rdump();
void		pdump();
void		init_memory();
void		init_inst_info();

/* YOU IMPLEMENT THIS FUNCTION in the run.c file */
void	process_instruction();

#endif
