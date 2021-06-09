#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define OP_J     2    // 1 J
#define OP_JAL   3    // 1 J
#define OP_JR    0    // 1 J
#define OP_LA    63   // 2 psuedo
// Immediate
// SignExtImm
#define OP_LW    35   // 2 I
#define OP_SW    43   // 2 I
#define OP_ADDIU 9    // 3 I
#define OP_SLTIU 11   // 3 I
// Simple
#define OP_LUI   15   // 2 I
// ZeroExtImm
#define OP_ANDI  12   // 3 I
#define OP_ORI   13   // 3 I
// Labels
#define OP_BEQ   4    // 3 I
#define OP_BNE   5    // 3 I

// Registers only
#define OP_AND   0    // 3 R
#define OP_NOR   0    // 3 R
#define OP_ADDU  0    // 3 R
#define OP_SUBU  0    // 3 R
#define OP_OR    0    // 3 R
#define OP_SLTU  0    // 3 R
// rd, rt, shamt
#define OP_SLL   0    // 3 R
#define OP_SRL   0    // 3 R

// Funct codes
#define FT_ADDU  33
#define FT_AND   36
#define FT_JR    8
#define FT_NOR   39
#define FT_OR    37
#define FT_SLTU  43
#define FT_SRL   2
#define FT_SLL   0
#define FT_SUBU  35
#define FT_SLTU  43

#define OP_EQ_RET(str1,str2,op) if(strcmp(str1,str2)==0)return op

typedef struct _word_map {
	char* block;
	int index;
} word_map;

typedef struct _instruction {
	int opcode;
	int funct;
	char* op1;
	char* op2;
	char* op3;
} instruction;

typedef struct _text_block {
	char* name;
	int index;
	int insts_count;
	int real_insts_count;
	instruction* insts;
} text_block;

int data_word_count = 0;
int data_block_count = 0;
word_map* data_maps = NULL;
int* words = NULL;

int text_block_count = 0;
int text_real_insts_count = 0;
text_block* text_blocks = NULL;

unsigned int resolve_data_addr(char* block, int offset) {
	for (int i=0; i<data_block_count; i++) {
		if (strcmp(data_maps[i].block, block)==0) {
			return ((1<<28) + 4 * (data_maps[i].index + offset));
		}
	}
	return 0;
}

int resolve_text_offset(char* block) {
	for (int i=0; i<text_block_count; i++) {
		if (strcmp(text_blocks[i].name, block)==0) {
			return text_blocks[i].index;
		}
	}
}

unsigned int resolve_text_addr(char* block) {
	return (1<<20) + resolve_text_offset(block);
}

int parse_opcode(char* str) {
	OP_EQ_RET(str, "addiu", OP_ADDIU);
	OP_EQ_RET(str, "la", OP_LA);
	OP_EQ_RET(str, "addu", OP_ADDU);
	OP_EQ_RET(str, "and", OP_AND);
	OP_EQ_RET(str, "andi", OP_ANDI);
	OP_EQ_RET(str, "beq", OP_BEQ);
	OP_EQ_RET(str, "bne", OP_BNE);
	OP_EQ_RET(str, "j", OP_J);
	OP_EQ_RET(str, "jal", OP_JAL);
	OP_EQ_RET(str, "jr", OP_JR);
	OP_EQ_RET(str, "lui", OP_LUI);
	OP_EQ_RET(str, "lw", OP_LW);
	OP_EQ_RET(str, "nor", OP_NOR);
	OP_EQ_RET(str, "or", OP_OR);
	OP_EQ_RET(str, "ori", OP_ORI);
	OP_EQ_RET(str, "sltiu", OP_SLTIU);
	OP_EQ_RET(str, "sltu", OP_SLTU);
	OP_EQ_RET(str, "sll", OP_SLL);
	OP_EQ_RET(str, "srl", OP_SRL);
	OP_EQ_RET(str, "sw", OP_SW);
	OP_EQ_RET(str, "subu", OP_SUBU);
	return -1;
}

int parse_funct(char* str) {
	OP_EQ_RET(str, "addu", FT_ADDU);
	OP_EQ_RET(str, "and", FT_AND);
	OP_EQ_RET(str, "jr", FT_JR);
	OP_EQ_RET(str, "nor", FT_NOR);
	OP_EQ_RET(str, "or", FT_OR);
	OP_EQ_RET(str, "sltu", FT_SLTU);
	OP_EQ_RET(str, "srl", FT_SRL);
	OP_EQ_RET(str, "sll", FT_SLL);
	OP_EQ_RET(str, "subu", FT_SUBU);
	OP_EQ_RET(str, "sltu", FT_SLTU);
	return -1;
}

int parse_register(char* reg) {
	return atoi(reg+1);
}

void print_bin_int(int v, int width) {
	for (int i=0; i<width; i++) {
		unsigned int k = v & (1U<<(width-1-i));
		printf("%d", k == 0 ? 0 : 1);
	}
	#ifdef OP_FORMAT
	printf(" ");
	#endif
}

void print_bin_r(int opcode, int rs, int rt, int rd, int shamt, int funct) {
	print_bin_int(opcode, 6);
	print_bin_int(rs, 5);
	print_bin_int(rt, 5);
	print_bin_int(rd, 5);
	print_bin_int(shamt, 5);
	print_bin_int(funct, 6);
	#ifdef OP_FORMAT
	printf("\n");
	#endif
}

void print_bin_i(int opcode, int rs, int rt, int immed) {
	print_bin_int(opcode, 6);
	print_bin_int(rs, 5);
	print_bin_int(rt, 5);
	print_bin_int(immed, 16);
	#ifdef OP_FORMAT
	printf("\n");
	#endif
}

void print_bin_j(int opcode, int addr) {
	print_bin_int(opcode, 6);
	print_bin_int(addr, 26);
	#ifdef OP_FORMAT
	printf("\n");
	#endif
}

void print_instruction(instruction* instr, int instr_offset) {
	int opcode = instr->opcode;
	int funct = instr->funct;
	if (opcode == OP_J || opcode == OP_JAL) {
		// J instruction
		print_bin_j(opcode, resolve_text_addr(instr->op1));
	} else if (opcode == OP_JR && funct == FT_JR) {
		print_bin_r(opcode, parse_register(instr->op1), 0, 0, 0, funct);
	} else if (opcode == 0 || opcode == OP_SLTU) {
		// R instruction
		if (funct == FT_SLL || funct == FT_SRL) {
			print_bin_r(
				opcode,
				0,
				parse_register(instr->op2),
				parse_register(instr->op1),
				(int)strtol(instr->op3, NULL, 0),
				funct
			);
		} else {
			print_bin_r(
				opcode,
				parse_register(instr->op2),
				parse_register(instr->op3),
				parse_register(instr->op1),
				0,
				funct
			);
		}
	} else if (opcode == OP_LA) {
		int la_addr = resolve_data_addr(instr->op2, 0);
		int reg = parse_register(instr->op1);
		print_bin_i(OP_LUI, 0, reg, la_addr >> 16);
		if (la_addr % (1<<16) != 0) {
			print_bin_i(OP_ORI, reg, reg, la_addr % (1<<16));
		}
	// I instructions from here
	} else if (opcode == OP_LUI) {
		print_bin_i(OP_LUI, 0, parse_register(instr->op1), (int)strtol(instr->op2, NULL, 0));
	} else if (opcode == OP_ANDI || opcode == OP_ORI || opcode == OP_ADDIU || opcode == OP_SLTIU) {
		print_bin_i(opcode, parse_register(instr->op2), parse_register(instr->op1), (int)strtol(instr->op3, NULL, 0));
	} else if (opcode == OP_LW || opcode == OP_SW) {
		char buf[512];
		strcpy(buf, instr->op2);
		int dollarPos = 0;
		for (;buf[dollarPos]!='$';dollarPos++) {}
		buf[dollarPos-1]=0;
		buf[strlen(instr->op2)-1]=0;
		int offset = atoi(buf);
		int reg = parse_register(buf+dollarPos);
		print_bin_i(opcode, reg, parse_register(instr->op1), offset);
	} else if (opcode == OP_BEQ || opcode == OP_BNE) {
		int change = resolve_text_offset(instr->op3) - instr_offset - 1;
		print_bin_i(
			opcode,
			parse_register(instr->op1),
			parse_register(instr->op2),
			change
		);
	}
}

void calculate_text_offset() {
	int total_real_insts_count = 0;
	for (int i=0; i<text_block_count; i++) {
		text_blocks[i].real_insts_count = 0;
		for (int j=0; j<text_blocks[i].insts_count; j++) {
			if (text_blocks[i].insts[j].opcode == OP_LA) {
				int la_addr = resolve_data_addr(text_blocks[i].insts[j].op2, 0);
				if (la_addr % (1<<16) == 0) {
					text_blocks[i].real_insts_count++;
				} else {
					text_blocks[i].real_insts_count+=2;
				}
			} else {
				text_blocks[i].real_insts_count++;
			}
		}
		text_blocks[i].index = total_real_insts_count;
		total_real_insts_count += text_blocks[i].real_insts_count;
		text_real_insts_count += text_blocks[i].real_insts_count;
	}
}

void data_segment() {
	char buf[512];	
	scanf("%s", buf);
	while(1) {
		if (strcmp(buf, ".text") == 0) {
			break;
		}
		buf[strlen(buf)-1]=0;
		char* block_name = (char*)malloc(strlen(buf)+1);
		strcpy(block_name, buf);
		int* block_words = (int*)malloc(sizeof(int));
		int block_cap = 1;
		int block_count = 0;
		while(1) {
			scanf("%s", buf);
			if (strcmp(buf, ".word")==0) {
				continue;
			}
			if (buf[0]<'0' || buf[0]>'9') {
				break;
			}
			if (block_count >= block_cap) {
				block_cap *= 2;
				block_words = (int*)realloc(block_words, sizeof(int)*block_cap);
			}
			block_words[block_count++]=(int)strtol(buf, NULL, 0);
		}
		// Append current block index
		word_map* nmap = (word_map*)malloc(sizeof(word_map));
		data_maps = (word_map*)realloc(data_maps, sizeof(word_map)*(data_block_count+1));
		data_maps[data_block_count].block = block_name;
		data_maps[data_block_count].index = data_word_count;
		data_block_count++;
		// Append current block words
		words = (int*)realloc(words, sizeof(int)*(data_word_count+block_count));
		memcpy(words+data_word_count, block_words, sizeof(int)*block_count);
		data_word_count += block_count;
		// Free: Don't free block_name
		free(block_words);
	}
}

void text_segment() {
	char buf[100], op1[100], op2[100], op3[100];
	if(scanf("%s", buf)==EOF) {
		return;
	}
	while(1) {
		int block_inst_count = 0;
		instruction* insts = NULL;
		char* block_name;
		buf[strlen(buf)-1]=0;
		block_name = (char*)malloc(sizeof(char)*strlen(buf)+1);
		strcpy(block_name, buf);
		int stop = 0;
		while(1) {
			if (scanf("%s", buf)==EOF) {
				stop = 1;
				break;
			}
			if (buf[strlen(buf)-1]==':') {
				break;
			}
			instruction inst;
			int opcode = parse_opcode(buf);
			int funct = parse_funct(buf);
			inst.opcode = opcode;
			inst.funct = funct;
			inst.op1 = inst.op2 = inst.op3 = NULL;
			if (opcode == OP_J || opcode == OP_JAL || (opcode == OP_JR && funct == FT_JR)) {
				scanf("%s", buf);
				inst.op1 = (char*)malloc(strlen(buf)+1);
				strcpy(inst.op1, buf);
			} else if (opcode == OP_LA || opcode == OP_LUI || opcode == OP_SW || opcode == OP_LW) {
				scanf("%s", buf);
				buf[strlen(buf)-1]=0;
				inst.op1 = (char*)malloc(strlen(buf)+1);
				strcpy(inst.op1, buf);
				scanf("%s", buf);
				inst.op2 = (char*)malloc(strlen(buf)+1);
				strcpy(inst.op2, buf);
			} else {
				scanf("%s", buf);
				buf[strlen(buf)-1]=0;
				inst.op1 = (char*)malloc(strlen(buf)+1);
				strcpy(inst.op1, buf);
				scanf("%s", buf);
				buf[strlen(buf)-1]=0;
				inst.op2 = (char*)malloc(strlen(buf)+1);
				strcpy(inst.op2, buf);
				scanf("%s", buf);
				inst.op3 = (char*)malloc(strlen(buf)+1);
				strcpy(inst.op3, buf);
			}
			insts = (instruction*)realloc(insts, sizeof(instruction)*(block_inst_count+1));
			insts[block_inst_count].opcode = inst.opcode;
			insts[block_inst_count].funct = inst.funct;
			insts[block_inst_count].op1 = inst.op1;
			insts[block_inst_count].op2 = inst.op2;
			insts[block_inst_count].op3 = inst.op3;
			block_inst_count++;
		}
		text_blocks = (text_block*)realloc(text_blocks, sizeof(text_block)*(text_block_count+1));
		text_blocks[text_block_count].insts = insts;
		text_blocks[text_block_count].insts_count = block_inst_count;
		text_blocks[text_block_count].name = block_name;
		text_block_count++;
		if (stop == 1) {
			break;
		}
	}
}

int main(int argc, char* argv[]){
	if(argc != 2){
		printf("Usage: ./runfile <assembly file>\n"); //Example) ./runfile /sample_input/example1.s
		printf("Example) ./runfile ./sample_input/example1.s\n");
		exit(0);
	}
	else
	{
		// To help you handle the file IO, the deafult code is provided.
		// If we use freopen, we don't need to use fscanf, fprint,..etc. 
		// You can just use scanf or printf function 
		// ** You don't need to modify this part **
		// If you are not famailiar with freopen,  you can see the following reference
		// http://www.cplusplus.com/reference/cstdio/freopen/

		//For input file read (sample_input/example*.s)

		char *file=(char *)malloc(strlen(argv[1])+3);
		strncpy(file,argv[1],strlen(argv[1]));

		if(freopen(file, "r",stdin)==0){
			printf("File open Error!\n");
			exit(1);
		}

		char buf[512];

		//From now on, if you want to read string from input file, you can just use scanf function.
		scanf("%s", buf);

		if (strcmp(buf, ".data")==0) {
			data_segment();
		}

		#ifdef DEBUG
		printf("%d %d\n", data_word_count, data_block_count);
		for (int i=0; i<data_block_count; i++) {
			printf("%3d %s\n", data_maps[i].index, data_maps[i].block);
		}
		for (int i=0; i<data_word_count; i++) {
			printf("%d\n", words[i]);
		}
		#endif

		// .text is read by data_segment()
		text_segment();
		calculate_text_offset();

		#ifdef DEBUG
		printf("text: %d %d\n", text_block_count, text_real_insts_count);
		for (int i=0; i<text_block_count; i++) {
			printf("%s %d %d %d\n",
				text_blocks[i].name,
				text_blocks[i].insts_count,
				text_blocks[i].real_insts_count,
				text_blocks[i].index
			);
			for (int j=0; j<text_blocks[i].insts_count; j++) {
				printf("  %d %s %s %s %d\n",
					text_blocks[i].insts[j].opcode,
					text_blocks[i].insts[j].op1,
					text_blocks[i].insts[j].op2,
					text_blocks[i].insts[j].op3,
					text_blocks[i].insts[j].funct
				);
			}
		}
		#endif

		file[strlen(file)-1] ='o';
		freopen(file, "w", stdout);

		print_bin_int(text_real_insts_count*4, 32);
		#ifdef OP_FORMAT
		printf("\n");
		#endif
		print_bin_int(data_word_count*4, 32);
		#ifdef OP_FORMAT
		printf("\n");
		#endif
		for (int i=0; i<text_block_count;i++) {
			#ifdef OP_FORMAT
			printf("\n");
			#endif
			for (int j=0; j<text_blocks[i].insts_count; j++) {
				print_instruction(&text_blocks[i].insts[j], text_blocks[i].index + j);
				fflush(stdout);
			}
		}
		for (int i=0; i<data_word_count; i++) {
			print_bin_int(words[i], 32);
		}
		fflush(stdout);
	}
	return 0;
}

