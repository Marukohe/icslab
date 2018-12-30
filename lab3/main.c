#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <assert.h>
#include <elf.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifndef __USE_GNU
# define __USE_GNU
#endif
#include <ucontext.h>
#undef __USE_GNU

#include <signal.h>

#define true 1
#define false 0
#define AS_START (uintptr_t)0x8048000
#define AS_SIZE (1 << 24)  // 16MB

// create an address space to loader the program
void init_address_space(void) {
  void *ret = mmap((void *)AS_START, AS_SIZE, PROT_EXEC | PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
  assert(ret != MAP_FAILED);

  uint32_t *p = ret;
  int i;
  for (i = 0; i < AS_SIZE / sizeof(uint32_t); i ++) {
    p[i] = rand();
  }
}

void bt(uint32_t ebp, uint32_t eip);

// SIGSEGV signal handler
void segv_handler(int signum, siginfo_t *siginfo, void *ucontext) {
  printf("catch SIGSEGV\n");

  // get ebp and eip from the context
  int *regs = ((ucontext_t *)ucontext)->uc_mcontext.gregs;
  uint32_t ebp = regs[REG_EBP];
  uint32_t eip = regs[REG_EIP];

  printf("ebp = %x, eip = %x\n", ebp, eip);

  bt(ebp, eip);

  exit(0);
}

// install signal handler
void init_signal() {
  struct sigaction s;
  memset(&s, 0, sizeof(s));
  s.sa_flags = SA_SIGINFO;
  s.sa_sigaction = segv_handler;
  int ret = sigaction(SIGSEGV, &s, NULL);
  assert(ret == 0);
}

void init_rand() {
  srand(time(0));
}

static char *strtab = NULL;
static Elf32_Sym *symtab = NULL;
static int nr_symtab_entry;
static Elf32_Phdr *ph = NULL;
uint32_t nr_prog_size;

// load symbol table and string table for future use
void load_elf_tables(char *filename) {
  int ret;

  FILE *fp = fopen(filename, "rb");
  assert(fp != NULL);

  uint8_t buf[sizeof(Elf32_Ehdr)];
  ret = fread(buf, sizeof(Elf32_Ehdr), 1, fp);
  assert(ret == 1);

  // the first several bytes contain the ELF header
  Elf32_Ehdr *elf = (void *)buf;
  char magic[] = {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3};

  // check ELF header
  assert(memcmp(elf->e_ident, magic, 4) == 0);		// magic number
  assert(elf->e_ident[EI_CLASS] == ELFCLASS32);		// 32-bit architecture
  assert(elf->e_ident[EI_DATA] == ELFDATA2LSB);		// littel-endian
  assert(elf->e_ident[EI_VERSION] == EV_CURRENT);		// current version
  assert(elf->e_ident[EI_OSABI] == ELFOSABI_SYSV || 	// UNIX System V ABI
      elf->e_ident[EI_OSABI] == ELFOSABI_LINUX); 	// UNIX - GNU
  assert(elf->e_ident[EI_ABIVERSION] == 0);			// should be 0
  assert(elf->e_type == ET_EXEC);						// executable file
  assert(elf->e_machine == EM_386);					// Intel 80386 architecture
  assert(elf->e_version == EV_CURRENT);				// current version

  //load program header table
  uint32_t ph_size = elf->e_phentsize * elf->e_phnum;
  nr_prog_size = ph_size/sizeof(ph[0]);
  ph = malloc(ph_size);
  fseek(fp, elf->e_phoff,SEEK_SET);
  ret = fread(ph, ph_size, 1, fp);
  assert(ret == 1);

  // load section header table
  uint32_t sh_size = elf->e_shentsize * elf->e_shnum;
  Elf32_Shdr *sh = malloc(sh_size);
  fseek(fp, elf->e_shoff, SEEK_SET);
  ret = fread(sh, sh_size, 1, fp);
  assert(ret == 1);

  // load section header string table
  char *shstrtab = malloc(sh[elf->e_shstrndx].sh_size);
  fseek(fp, sh[elf->e_shstrndx].sh_offset, SEEK_SET);
  ret = fread(shstrtab, sh[elf->e_shstrndx].sh_size, 1, fp);
  assert(ret == 1);

  int i;
  for(i = 0; i < elf->e_shnum; i ++) {
    if(sh[i].sh_type == SHT_SYMTAB && 
        strcmp(shstrtab + sh[i].sh_name, ".symtab") == 0) {
      // load symbol table
      symtab = malloc(sh[i].sh_size);
      fseek(fp, sh[i].sh_offset, SEEK_SET);
      ret = fread(symtab, sh[i].sh_size, 1, fp);
      assert(ret == 1);
      nr_symtab_entry = sh[i].sh_size / sizeof(symtab[0]);
    }
    else if(sh[i].sh_type == SHT_STRTAB && 
        strcmp(shstrtab + sh[i].sh_name, ".strtab") == 0) {
      // load string table
      strtab = malloc(sh[i].sh_size);
      fseek(fp, sh[i].sh_offset, SEEK_SET);
      ret = fread(strtab, sh[i].sh_size, 1, fp);
      assert(ret == 1);
    }
  }

  free(sh);
  free(shstrtab);

  assert(strtab != NULL && symtab != NULL);

  fclose(fp);
}

// TODO: implement the following functions

uintptr_t get_entry(void) {
	  for(int i=0;i<nr_symtab_entry;i++){
		if((symtab[i].st_info&0xf)!=STT_FUNC)
			continue;
	    char *name = strtab+symtab[i].st_name;
	 	if(strcmp(name,"main")==0){
			return symtab[i].st_value;
	 	}
	} 
  return 0;
}

int cnt=0;
void loader(char *filename) {
  int ret;
  int size;
  int i=0;

  FILE *fp = fopen(filename, "rb");
  assert(fp != NULL);

  fseek(fp,0,SEEK_END);
  size = ftell(fp);
  //printf("%d\n",size);
  fclose(fp);

  char *buf;
  fp = fopen(filename,"rb");
  buf = (char *)malloc(sizeof(char)*size);
  while(!feof(fp)){
	  ret=fscanf(fp,"%c",&buf[i]);
	  assert(ret==1||ret==-1);
	  i++;
  }
  //Elf32_Ehdr *elf = (void *)buf;

  for(int i=0; i<nr_prog_size;i++){
  	  if(ph[i]. p_type == PT_LOAD){
	  	  memcpy((void *)ph[i].p_vaddr,(const void *)(ph[i].p_offset+buf),ph[i].p_filesz);
		  memset((void *)ph[i].p_vaddr+ph[i].p_filesz,0,ph[i].p_memsz-ph[i].p_filesz);
	  }
  }
}

char *getname(uint32_t offset){
	for(int i=0;i<nr_symtab_entry;i++){
		if(symtab[i].st_value<=offset&&offset<=symtab[i].st_value+symtab[i].st_size){
			if((symtab[i].st_info&0xf)!=STT_FUNC)
				continue;
			char *ret = strtab+symtab[i].st_name;
			return ret;
		}
	}
	return NULL;
}

void bt(uint32_t ebp, uint32_t eip) {
	int num = 0;
	while(ebp!=0)	{
		char *ret = getname(eip);
		uint32_t *args;
		args = (uint32_t *)ebp+2;
		if(ret!=NULL){
			printf("#%d  0x%x in %s()  ",num++,eip,ret);
			printf("(%d %d %d %d)\n",args[0],args[1],args[2],args[3]);
		}
		else{
			printf("#%d  0x%x in ???  ",num++,eip);
			printf("(%d %d %d %d)\n",args[0],args[1],args[2],args[3]);
		}
		eip = *((uint32_t *)ebp+1);
		ebp = *(uint32_t *)ebp;
	}
}

int main(int argc, char *argv[]) {
  assert(argc >= 2);

  init_signal();

  init_rand();

  init_address_space();

  load_elf_tables(argv[1]);

  uintptr_t entry = get_entry();

  printf("entry = 0x%x\n", entry);

  loader(argv[1]);

  ((void (*) (void))entry)();

  return 0;
}
