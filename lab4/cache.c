#include "common.h"
#include <inttypes.h>

void mem_read(uintptr_t block_num, uint8_t *buf);
void mem_write(uintptr_t block_num, const uint8_t *buf);

//void mem_uncache_write(uintptr_t addr, uint32_t data, uint32_t wmask);
//uint32_t mem_uncache_read(uintptr_t addr);

static uint64_t cycle_cnt = 0;
static uint64_t misscnt = 0;
static uint64_t allcnt = 0;

static uint64_t cachemodel;

void cycle_increase(int n) { cycle_cnt += n; }

// TODO: implement the following functions

typedef struct{
	bool valid_bit;
	bool dirty_bit;
	//uint64_t data=0;
	uint8_t data[BLOCK_SIZE];
	uint64_t tag;
}Cache_line;

typedef struct{
	Cache_line *lines;
}Cache_set;

typedef struct{
	int S;
	int C;
	int ts;
	Cache_set *set;
}Cache;

Cache *caches;

uint32_t read_res(int setnum,int linnum,int addr){
	uint32_t offset = (1<<BLOCK_WIDTH)-1;
	return (uint32_t)(caches->set[setnum].lines[linnum].data[addr&offset])+(uint32_t)(caches->set[setnum].lines[linnum].data[(addr&offset)+1])*(1<<8)+(uint32_t)(caches->set[setnum].lines[linnum].data[(addr&offset)+2])*(1<<16)+(uint32_t)(caches->set[setnum].lines[linnum].data[(addr&offset)+3])*(1<<24);
}

void write_res(int setnum, int linnum, uintptr_t addr, uint32_t data, uint32_t wmask){
	uint32_t offset = (1<<BLOCK_WIDTH)-1;
	data &=wmask;
	caches->set[setnum].lines[linnum].data[(addr&offset)+3] =(caches->set[setnum].lines[linnum].data[(addr&offset)+3]&((~wmask)>>24))|(data>>24);
	data<<=8;
	wmask<<=8;	
	caches->set[setnum].lines[linnum].data[(addr&offset)+2] =(caches->set[setnum].lines[linnum].data[(addr&offset)+2]&((~wmask)>>24))|(data>>24);
	data<<=8;
	wmask<<=8;
	caches->set[setnum].lines[linnum].data[(addr&offset)+1] =(caches->set[setnum].lines[linnum].data[(addr & offset)+1]&((~wmask)>>24))|(data>>24);
	data<<=8;
	wmask<<=8;
	caches->set[setnum].lines[linnum].data[(addr&offset)] =(caches->set[setnum].lines[linnum].data[(addr & offset)]&((~wmask)>>24))|(data>>24);

}

uint32_t cache_read(uintptr_t addr) {
	//cycle_increase(cachemodel);
	//printf("read:  %lx\n",addr);
	//assert(addr!=0x7ffc);
	allcnt++;
	addr &=(~0x3);
	int setnum = (addr&((1<<(caches->ts+BLOCK_WIDTH))-1))>>BLOCK_WIDTH; 
	int tag = addr>>(caches->ts+BLOCK_WIDTH);

	for(int i=0;i<caches->C;i++) {
		cycle_increase(1);
		if(caches->set[setnum].lines[i].valid_bit == 1 && caches->set[setnum].lines[i].tag == tag){
			//assert(addr!=0x7fec);
			return read_res(setnum,i,addr);
	 	}
	}  
    //====================================
	//read do not hit
	//===================================
	misscnt++;
	bool flag = false;
	for(int i= 0;i<caches->C;i++){
		cycle_increase(1);
	 	if(caches->set[setnum].lines[i].valid_bit == 0){
			mem_read(addr>>BLOCK_WIDTH,caches->set[setnum].lines[i].data);
			caches->set[setnum].lines[i].valid_bit = 1;
			caches->set[setnum].lines[i].tag = tag;
			//assert(addr!=0x7fec);
			flag = true;
			//assert(0);
			return read_res(setnum,i,addr);
	 	}
	} 
	if(!flag){ 
		//assert(addr!=0x7fec);
		int rline = rand()%caches->C;
		mem_write((caches->set[setnum].lines[rline].tag<<caches->ts)+setnum,caches->set[setnum].lines[rline].data);
		mem_read(addr>>BLOCK_WIDTH,caches->set[setnum].lines[rline].data);
		caches->set[setnum].lines[rline].valid_bit = 1;
		caches->set[setnum].lines[rline].tag = tag;	
		return read_res(setnum,rline,addr);
	}  
	assert(0);
	return 0;
}

//==============================================
//cache_write
//==============================================

void cache_write(uintptr_t addr, uint32_t data, uint32_t wmask) {
	//cycle_increase(cachemodel);
	//printf("write: %lx\n",addr);
	allcnt++;
	addr &=(~0x3);
	int setnum = (addr&((1<<(caches->ts+BLOCK_WIDTH))-1))>>BLOCK_WIDTH;
	int tag = addr>>(caches->ts+BLOCK_WIDTH);
	int block_num = addr>>BLOCK_WIDTH;
	//hit
	for(int i=0;i<caches->C;i++){ 
		cycle_increase(1);
		if(caches->set[setnum].lines[i].tag == tag&&caches->set[setnum].lines[i].valid_bit == 1){
				write_res(setnum, i, addr, data, wmask);
				caches->set[setnum].lines[i].dirty_bit = 1;
				//assert(cache_read(addr)==mem_uncache_read(addr));
				return;
	 	}
	} 
	//==========================================
	//write can not hit
	//===========================================
	misscnt++;
	for(int i=0;i<caches->C;i++){ 
		cycle_increase(1);
		if(caches->set[setnum].lines[i].valid_bit==0){
			//assert(0);
			mem_read(block_num,caches->set[setnum].lines[i].data);
			//uint8_t *p = (uint8_t *)caches->set[setnum].lines[i].data+(addr & 0x3f);
			write_res(setnum,i,addr,data,wmask);
			caches->set[setnum].lines[i].dirty_bit = 0;
			caches->set[setnum].lines[i].tag = tag;
			caches->set[setnum].lines[i].valid_bit = 1;
			mem_write(block_num,caches->set[setnum].lines[i].data);
			//assert(cache_read(addr)==mem_uncache_read(addr));
			//printf("\n%x\n\n",cache_read(addr));
			return;
		} 
	}
	int rline = rand()%caches->C;
	if(caches->set[setnum].lines[rline].dirty_bit==1)
		mem_write((caches->set[setnum].lines[rline].tag<<caches->ts)+setnum,caches->set[setnum].lines[rline].data);
	mem_read(block_num,caches->set[setnum].lines[rline].data);


	write_res(setnum, rline, addr, data, wmask);

	caches->set[setnum].lines[rline].dirty_bit = 0;
	caches->set[setnum].lines[rline].tag = tag;
	caches->set[setnum].lines[rline].valid_bit = 1;
	mem_write(block_num,caches->set[setnum].lines[rline].data);

	//assert(0);
	//assert(cache_read(addr)==mem_uncache_read(addr));
}

void init_cache(int total_size_width, int associativity_width) {
	cachemodel = 1<<associativity_width;
	caches = (Cache*)malloc(sizeof(Cache));
	caches->ts = total_size_width-BLOCK_WIDTH-associativity_width;
	caches->S = exp2(total_size_width-BLOCK_WIDTH-associativity_width); //组数
	caches->C = 1<<associativity_width; //每组行数
	caches->set = (Cache_set*)malloc(caches->S*sizeof(Cache_set));

	for(int i=0;i<caches->S;i++){
		caches->set[i].lines = (Cache_line *)malloc(caches->C*sizeof(Cache_line));
		for(int j=0;j<caches->C;j++){
			//caches->set[i].lines[j].data = (uint8_t *)mallc(sizeof())
			caches->set[i].lines[j].valid_bit = 0;
			caches->set[i].lines[j].tag = 0;
		}
	}  
	return;
}

void display_statistic(void) {
	printf("cycle_cnt: %ld  ",cycle_cnt);
	printf("misscnt: %ld  ",misscnt);
	printf("allcnt: %ld  ",allcnt);
	printf("RATE: %lf\n",1-((double)misscnt)/((double)allcnt));
}
