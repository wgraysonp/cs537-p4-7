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
	
	int new_addr;
	int length;
	int flags;
	//int fd;

	if(argint(0, &new_addr) < 0 || argint(1, &length) < 0 || argint(2, &flags) < 0){
		return -1;
	}
	new_addr = (uint)new_addr;



	struct proc *curproc = myproc();
	uint curr_addr;
	int curr_pages;

	for (int i = 0; i < MAX_WMAP; i++){
		if (curproc->wmaps[i] == 0){
			struct map *new_map;
			if ((new_map = mapalloc())==0){
				return FAILED;
			}
			new_map->addr = new_addr;
			new_map->pages = length/4096;
			new_map->mapshared = 1;
			new_map->fd = -1;
			new_map->n_alloc_pages = 0;
			curproc->wmaps[i] = new_map;
			return SUCCESS;
		} else {
			curr_addr = curproc->wmaps[i]->addr;
			curr_pages = curproc->wmaps[i]->pages;
			if(curr_addr <= new_addr && new_addr < curr_addr + 4096*curr_pages){
				// pages already allocated
				return FAILED;
			}
		}
	}

	return FAILED;
}

int sys_wunmap(void){
	return 0;
}
