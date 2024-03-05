// Memory mapping system calls


#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "wmap.h"

// Each process now has an array wmaps that can be accessed by p->wmaps.
// wmaps has length MAX_WMAP=16. Each entry is a pointer to a struct of 
// type `map` containing metadata for one memory mapping. struct `map` is
// defined in wmap.h

int sys_wmap(void){
	
	int addr;
	int length;
	int flags;
	//int fd;

	if(argint(0, &addr) < 0 || argint(1, &length) < 0 || argint(2, &flags) < 0){
		return -1;
	}
	addr = (uint)addr;

	struct proc *curproc = myproc();
	if (curproc->wmaps[0] == 0){
		return 100;
	}

	return flags;
}

int sys_wunmap(void){
	return 0;
}
