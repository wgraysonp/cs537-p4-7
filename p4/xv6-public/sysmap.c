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
// defined in wmap.h


int sys_wmap(void){
	
	int new_addr;
	int length;
	int flags;
	uint base = 0x60000000;
	//int fd;

	if(argint(0, &new_addr) < 0 || argint(1, &length) < 0 || argint(2, &flags) < 0){
		return -1;
	}

	if ((flags & MAP_SHARED) && (flags & MAP_PRIVATE)){
		return -1;
	}

	if (flags & MAP_FIXED){
		new_addr = (uint)new_addr;
		if (new_addr % 4096 != 0){
			return -1;
		} else if (new_addr < base || new_addr >= KERNBASE){
			return -1;
		}
	} else {
		pte_t *pte;
		uint curr_page = base;
		int found = 0;
		int num_pages = (length/4096) + (length % 4096 > 0);
		while (curr_page < KERNBASE){
			if ((pte = walkpgdir(myproc()->pgdir, (void*)curr_page, 0)) == 0){
				// found an available page staring at new_addr
				// now check to see if remainig pages are available
				new_addr = curr_page;
				int map_available = 1;
				for (int i = 1; i < num_pages; i++){
					// check to make sure all requested pages starting at new_addr are
					// available
					curr_page = curr_page + 4096*i;
					if ((pte = walkpgdir(myproc()->pgdir, (void*)curr_page, 0)) != 0){
						cprintf("curr_page: 0x%x\n", curr_page);
						// not all pages requested are available starting at new_addr
						map_available = 0;
						break;
					}
				}
				if (map_available == 1){
					// made it through the loop so the mapping staring at new_addr is available
					found = 1;
					break;
				}
			}
		}
	       	
		if (found == 0){
			// made it through the whole loop without finding available room 
			// so the request fails
			return -1;
		}
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
			new_map->addr = new_addr;
			new_map->pages = length/4096;
			new_map->fd = -1;
			new_map->n_alloc_pages = 0;
			if (flags & MAP_SHARED){
				new_map->mapshared = 1;
			}
			curproc->wmaps[i] = new_map;
			return new_addr;
		} else {
			curr_addr = curproc->wmaps[i]->addr;
			curr_pages = curproc->wmaps[i]->pages;
			if(curr_addr <= new_addr && new_addr < curr_addr + 4096*curr_pages){
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
				unmap(curproc->wmaps[i]);
				curproc->wmaps[i] = 0;
				return 0;
			}
		}
	}

	return -1;
}
