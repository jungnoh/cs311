#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BYTES_PER_WORD 4
// Block size is at least 4 bytes; we can use the lower two bits to store additional flags.
#define EMPTY_BIT 0x1
#define DIRTY_BIT 0x2

#define TAG(addr) (addr / (blocksize * set_count))
#define SET(addr) ((addr / blocksize) & (set_count - 1))

uint32_t** cache;
uint32_t** lru;
int capacity = 256;
int way = 4;
int blocksize = 8;
char dump_content = 0;
int set_count;
int words;

int cnt_read = 0;
int cnt_write = 0;
int cnt_wb = 0;
int cnt_hit_read = 0;
int cnt_hit_write = 0;
int cnt_miss_read = 0;
int cnt_miss_write = 0;
int cycle = 0;

/***************************************************************/
/*                                                             */
/* Procedure : cdump                                           */
/*                                                             */
/* Purpose   : Dump cache configuration                        */   
/*                                                             */
/***************************************************************/
void cdump(int capacity, int assoc, int blocksize) {
	printf("Cache Configuration:\n");
  printf("-------------------------------------\n");
	printf("Capacity: %dB\n", capacity);
	printf("Associativity: %dway\n", assoc);
	printf("Block Size: %dB\n", blocksize);
	printf("\n");
}

/***************************************************************/
/*                                                             */
/* Procedure : sdump                                           */
/*                                                             */
/* Purpose   : Dump cache stat		                             */   
/*                                                             */
/***************************************************************/
void sdump() {
	printf("Cache Stat:\n");
  printf("-------------------------------------\n");
	printf("Total reads: %d\n", cnt_read);
	printf("Total writes: %d\n", cnt_write);
	printf("Write-backs: %d\n", cnt_wb);
	printf("Read hits: %d\n", cnt_hit_read);
	printf("Write hits: %d\n", cnt_hit_write);
	printf("Read misses: %d\n", cnt_miss_read);
	printf("Write misses: %d\n", cnt_miss_write);
	printf("\n");
}


/***************************************************************/
/*                                                             */
/* Procedure : xdump                                           */
/*                                                             */
/* Purpose   : Dump current cache state                        */
/*                                                             */
/* Cache Design						                                     */ 
/*  							                                             */
/* 	    cache[set][assoc][word per block]		                   */
/*      						  						  			  						       */
/*      						  						  			  						       */
/*       ----------------------------------------	             */
/*       I        I  way0  I  way1  I  way2  I                 */
/*       ----------------------------------------              */
/*       I        I  word0 I  word0 I  word0 I                 */
/*       I  set0  I  word1 I  word1 I  work1 I                 */
/*       I        I  word2 I  word2 I  word2 I                 */
/*       I        I  word3 I  word3 I  word3 I                 */
/*       ----------------------------------------              */
/*       I        I  word0 I  word0 I  word0 I                 */
/*       I  set1  I  word1 I  word1 I  work1 I                 */
/*       I        I  word2 I  word2 I  word2 I                 */
/*       I        I  word3 I  word3 I  word3 I                 */
/*       ----------------------------------------              */
/*      						  						  			  						       */
/*                                                             */
/***************************************************************/
void xdump(int set, int way, uint32_t** cache)
{
	int i,j,k = 0;

	printf("Cache Content:\n");
	printf("-------------------------------------\n");
	for(i = 0; i < way;i++) {
		if(i == 0) {
			printf("    ");
		}
		printf("      WAY[%d]",i);
	}
	printf("\n");

	for (i = 0 ; i < set; i++) {
		printf("SET[%d]:   ", i);
		for (j = 0; j < way; j++) {
			if (k != 0 && j == 0) {
				printf("          ");
			}
			printf("0x%08x  ", cache[i][j]);
		}
		printf("\n");
	}
	printf("\n");
}

int evict(int set) {
	int result = 0;
	for (int i=0; i<way; i++) {
		if (lru[set][i] < lru[set][result]) {
			result = i;
		}
	}
	if (cache[set][result] & DIRTY_BIT) {
		// Write-back
		cnt_wb++;
	}
	return result;
}

int check_hit(uint32_t addr) {
	for (int i=0; i<way; i++) {
		if (cache[SET(addr)][i] != EMPTY_BIT && TAG(cache[SET(addr)][i]) == TAG(addr)) {
			return i;
		}
	}
	return -1;
}


// Process a single memory operation.
// rw: read 0, write 1
void process(char rw, uint32_t addr) {
	cycle++;
	addr &= ~(blocksize - 1);
	if (rw == 0) {
		cnt_read++;
		int hit = check_hit(addr);
		if (hit == -1) {
			// Cache miss
			cnt_miss_read++;
			hit = evict(SET(addr));
			cache[SET(addr)][hit] = addr;
		} else {
			// Cache hit
			cnt_hit_read++;
		}
		lru[SET(addr)][hit] = cycle;
		return;
	}
	cnt_write++;
	int hit = check_hit(addr);
	if (hit == -1) {
		// Cache miss: write-allocate
		cnt_miss_write++;
		hit = evict(SET(addr));
		cache[SET(addr)][hit] = addr;
	} else {
		// Cache hit
		cnt_hit_write++;
	}
	cache[SET(addr)][hit] |= DIRTY_BIT;
	lru[SET(addr)][hit] = cycle;
}


int main(int argc, char *argv[]) {
	int i, j, k;	

	// Parse argv
	if (strncmp(argv[1], "-c", 3)) {
		return -1;
	}

	int delIndex1=0, delIndex2=0;
	for (int i=0; i<2; i++) {
		int j;
		for (j=(i==0?0:delIndex1+1); argv[2][j] && (argv[2][j] != ':'); j++);
		if (i == 0) {
			delIndex1 = j;
		} else {
			delIndex2 = j;
		}
	}
	argv[2][delIndex1] = argv[2][delIndex2] = 0;
	capacity = atoi(argv[2]);
	way = atoi(argv[2]+delIndex1+1);
	blocksize = atoi(argv[2]+delIndex2+1);

	set_count = capacity / way / blocksize;
	words = blocksize / BYTES_PER_WORD;	

	if (strncmp(argv[3], "-x", 3) == 0) {
		dump_content = 1;
	}

	if (freopen(argv[dump_content ? 4 : 3], "r", stdin) == 0) {
		return -1;
	}

	// Allocate
	cache = (uint32_t**) malloc(sizeof(uint32_t*) * set_count);
	lru = (uint32_t**) malloc(sizeof(uint32_t*) * set_count);
	for(i = 0; i < set_count; i++) {
		cache[i] = (uint32_t*) malloc(sizeof(uint32_t) * way);
		lru[i] = (uint32_t*) malloc(sizeof(uint32_t) * way);
	}
	for(i = 0; i < set_count; i++) {
		for(j = 0; j < way; j ++) {
			cache[i][j] = EMPTY_BIT;
			lru[i][j] = 0;
		}
	}

	char op[2], addr[11];
	while (scanf("%1s %10s", op, addr) != EOF) {
		uint32_t addr_int = strtoul(addr, NULL, 0);
		process(op[0] == 'R' ? 0 : 1, addr_int);
	}

	// Clear additional bits for output
	for(i = 0; i < set_count; i++) {
		for(j = 0; j < way; j ++) {
			cache[i][j] &= ~(EMPTY_BIT | DIRTY_BIT);
		}
	}

	// Test example
	cdump(capacity, way, blocksize);
	sdump(0, 0, 0, 0, 0, 0, 0); 
	if (dump_content) {
		xdump(set_count, way, cache);
	}

	return 0;
}
