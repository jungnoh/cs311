# Project 3. MIPS Pipelined Simulator
Submission by Junghoon Noh for KAIST CS311 Computer Organization.

# Implementation details

## 1. util.h
Usage for every field in the `CPU_State` is commented extensively in code.
Still, there are some notable changes:
- Control outputs have their own struct (`Control_WB`, `Control_MEM`, `Control_EX`) for better organization and to ease passing between latches.
- `PIPE_STALL` was removed in favor of more explicit stall bits (`IF_STALL`, `ID_STALL`, `EX_STALL`)
- The ID stage only loads the instruction to a `uint32_t` value, which cannot be directly decoded. Instead, it is converted to an `instruction` struct at the start of the next cycle.

## 2. run.h
Although additional functions were added to `run.c` to process each pipeline step of a CPU cycle,
these were not added to this file since they aren't used anywhere else other than `run.c`.

## 3. run.c
Each pipeline cycles is split into these steps.
### Initializing 
This step initializes CPU state variables that is used within a single cycle.
This includes branch mux selections, PC, latch activation bits, etc.

### Forwarding
R-type EX/MEM → EX, R-type MEM/WB → EX, MEM/WB → MEM forwarding are implemented as described in the lecture.

In addition to these, I-type forwardings to EX had to be added to fully mitigate EX stage bubbles.
Not all I-type instructions can be forwarded, however, since branch instructions are also I-type. Only ADDIU, ANDI, LUI, ORI, SLTIU operations are forwarded.

This is what the complete EX/MEM → EX logic looks like:
```c
// EX/MEM -> EX
if (CURRENT_STATE.EX_MEM_CTRL_WB.reg_write) {
    uint32_t op = OPCODE(&CURRENT_STATE.EX_MEM_INSTR);
    if (op == 0) {
        if (RD(&CURRENT_STATE.EX_MEM_INSTR) != 0 && RD(&CURRENT_STATE.EX_MEM_INSTR) == RS(&CURRENT_STATE.ID_EX_INSTR)) {
            // Foward OP1 EX_MEM RD->RS
        }
        if (RD(&CURRENT_STATE.EX_MEM_INSTR) != 0 && RD(&CURRENT_STATE.EX_MEM_INSTR) == RT(&CURRENT_STATE.ID_EX_INSTR)) {
            // Foward OP2 EX_MEM RD->RT
        }
    } else if (op == OP_ADDIU || op == OP_ANDI || op == OP_LUI || op == OP_ORI || op == OP_SLTIU) {
        if (RT(&CURRENT_STATE.EX_MEM_INSTR) != 0 && RT(&CURRENT_STATE.EX_MEM_INSTR) == RS(&CURRENT_STATE.ID_EX_INSTR)) {
            // Foward OP1 EX_MEM RT->RS
        }
        if (RT(&CURRENT_STATE.EX_MEM_INSTR) != 0 && RT(&CURRENT_STATE.EX_MEM_INSTR) == RT(&CURRENT_STATE.ID_EX_INSTR)) {
            // Foward OP2 EX_MEM RT->RT
        }
    }
}
```
MEM/WB → EX forwarding is also written in the same manner.

### Process pipeline steps
Run the five pipeline steps. Latch updates - for example writing EX/MEM latches in the EX stage - are done during this process.
The pipeline steps are done in WB, MEM, EX, ID, IF
order to prevent latch overwriting and register structural hazards.

### Update PC
PC candidate values and PC mux selection bits are chosen during the pipeline steps.
These need to be evaluated to select the final PC.

### Finishing up
- Control bits to be used in the next cycle are written (`T_PC_WRITE`, `T_IF_ID_WRITE`)
- `RUN_BIT` is set to zero if there are no more instructions to read and the pipeline is empty.

## 4. cs311.c
The following line was removed in accordance to changes in `CPU_State`.
```c
CURRENT_STATE.PIPE_STALL[i] = FALSE;
```

