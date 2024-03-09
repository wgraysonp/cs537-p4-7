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
#include "memlayout.h"

// Each process now has an array wmaps that can be accessed by p->wmaps.
// wmaps has length MAX_WMAP=16. Each entry is a pointer to a struct of 
// type `map` containing metadata for one memory mapping. struct `map` is
// defined in wmap.h. To remove a mapping p->wmaps[i] set all fields to zero 
// then set p->wmaps[i] = 0 so the process no longer has a reference to that
// mapping struct


int sys_wmap(void){
	
	int new_addr;
	int length;
	int flags;
	int fd;
	struct file *f;

	if(argint(0, &new_addr) < 0 || argint(1, &length) < 0 || argint(2, &flags) < 0 || argint(3, &fd) < 0){
		return -1;
	}

	int num_pages = (length/4096) + (length % 4096 > 0);

	if ((flags & MAP_SHARED) && (flags & MAP_PRIVATE)){
		return -1;
	}

	if (!(flags & MAP_ANONYMOUS) && (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)){
		return -1;
	}


	struct proc *curproc = myproc();
	uint curr_addr;
	int curr_pages;

	for (int i = 0; i < MAX_WMAP; i++){
		if (curproc->wmaps[i] == 0){
			struct map *new_map;
			if ((new_map = mapalloc())==0){
				cprintf("map alloc failed\n");
				return -1;
			}
			uint addr = get_addr(new_addr, num_pages, flags);
			if (addr == -1){
				return -1;
			}
			new_map->addr = addr;
			new_map->pages = num_pages;
			new_map->fd = -1;
			new_map->n_alloc_pages = 0;
			new_map->size = length;
			if (flags & MAP_SHARED){
				new_map->mapshared = 1;
			}
			if (flags & MAP_ANONYMOUS){
				new_map->fd = -1;
			} else {
				new_map->fd = fd;
			}
			curproc->wmaps[i] = new_map;
			return addr;
		} else {
			curr_addr = curproc->wmaps[i]->addr;
			curr_pages = curproc->wmaps[i]->pages;
			if((flags & MAP_FIXED) && (curr_addr <= new_addr && new_addr < curr_addr + 4096*curr_pages)){
				// pages already allocated
				cprintf("page already allocated\n");
				return -1;
			}
		}
	}
	cprintf("made it to the end\n");
	return -1;
}

int sys_wunmap(void){
	int addr;
	if (argint(0, &addr) < 0){
		return -1;
	}

	struct proc *curproc = myproc();
	uint curr_addr;
	int curr_pages;

	for (int i = 0; i < MAX_WMAP; i++){
		if (curproc->wmaps[i] != 0){
			curr_addr = curproc->wmaps[i]->addr;
			curr_pages = curproc->wmaps[i]->pages;
			if (curr_addr <= addr && addr < curr_addr + 4096*curr_pages){
				if(unmap(curproc->wmaps[i]) == -1)
					return -1;
				curproc->wmaps[i] = 0;
				return 0;
			}
		}
	}

	return -1;
}
