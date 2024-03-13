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
#include <stdio.h>

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
			new_map->cpid = curproc->pid;
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

int sys_getpgdirinfo(void) {
	struct pgdirinfo *pdinfo;
	struct proc *currproc = myproc();
	argptr(0, (char**)&pdinfo, sizeof(struct pgdirinfo));
	memset(pdinfo, 0, sizeof(*pdinfo));

	uint va;
	pte_t *pte;
	for (va = 0; va < KERNBASE; va+= 4096) {
		pte = walkpgdir(currproc->pgdir, (void*)va, 0);
		// check if page is user-accessible
		if (pte && (*pte & PTE_U)) {
			uint pages = pdinfo->n_upages;
			pdinfo->va[pages] = va;
			pdinfo->pa[pages] = PTE_ADDR(*pte);
			pdinfo->n_upages++;
		}

	}
	return 0;
}


int sys_getwmapinfo(void) {
	struct wmapinfo *wminfo;
	struct proc *currproc = myproc();
	argptr(0, (char**)&wminfo, sizeof(struct wmapinfo));

	// count used mappings
	int count = 0;
	for (int i = 0; i < MAX_WMMAP_INFO; i++) {
		if (currproc->wmaps[i] && currproc->wmaps[i]->used) {
			wminfo->addr[count] = currproc->wmaps[i]->addr;
			wminfo->length[count] = currproc->wmaps[i]->size;
			wminfo->n_loaded_pages[count] = currproc->wmaps[i]->n_alloc_pages;
			count++;
		}
	}
	wminfo->total_mmaps = count; 
	return 0;
}



// used in sys_wremap for finding new location
uint new_addr(struct proc *currproc, int newsize) {
	uint base = 0x60000000;
	for (int i = 0; i < MAX_WMMAP_INFO; i++) {
		// check if newsize fits in free space before next mapping
		if ((base + newsize <= currproc->wmaps[i]->addr) && (base + newsize <= KERNBASE)) {
			return base; // free space found
		} else {
			base = currproc->wmaps[i]->addr + currproc->wmaps[i]->size; // move to next free space
		}
	}
	if (base + newsize <= KERNBASE) {
		return base;
	}
	return 0; // free space not found
} 


int sys_wremap(void) {
	struct proc *currproc = myproc();
	uint oldaddr;
	int oldsize;
	int newsize;
	int flags;
	int grow_in_place = 1; 

	if (argint(0, (int *)&oldaddr) < 0 || argint(1, &oldsize) < 0 || argint(2, &newsize) < 0 || argint(3, &flags) < 0){
                return -1;
        }

	uint oldaddr_end = oldaddr + oldsize;
        uint newaddr_end = oldaddr + newsize;

	for (int i = 0; i < MAX_WMMAP_INFO; i++) {
		// check if space is available for mapping to grow in place
		if (currproc->wmaps[i] && (currproc->wmaps[i]->addr >= oldaddr_end) && (currproc->wmaps[i]->addr <= newaddr_end)) {
			grow_in_place = 0; // space unavailable
		}
		// check if newaddr is between 0 and KERNBASE
		if (newaddr_end >= KERNBASE) {
			grow_in_place = 0; // exceeds KERNBASE
		}

		if (currproc->wmaps[i] && (currproc->wmaps[i]->addr == oldaddr) && (currproc->wmaps[i]->size == oldsize)) {
			// growing
			if (newsize > oldsize) { 
				if (grow_in_place) {
					currproc->wmaps[i]->size = newsize;
					currproc->wmaps[i]->pages = (newsize / 4096) + (newsize % 4096 > 0);
					return oldaddr;
				}
				else if (flags & MREMAP_MAYMOVE) {
					uint newaddr = new_addr(currproc, newsize);
					if (newaddr == 0) {
						//return -1; // no free space found
						return FAILED;
					} else {
						currproc->wmaps[i]->addr = newaddr;
						currproc->wmaps[i]->size = newsize;
                                        	currproc->wmaps[i]->pages = (newsize / 4096) + (newsize % 4096 > 0);
						return newaddr;
					}
				} else {
				return FAILED;
				}
			// shrinking
			} else if (newsize < oldsize) {
				currproc->wmaps[i]->size = newsize;
				currproc->wmaps[i]->pages = (newsize / 4096) + (newsize % 4096 > 0);
				return oldaddr;
			}
		}
	}
	return FAILED;
}
