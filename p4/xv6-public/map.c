#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "wmap.h"
#include "mmu.h"
#include "proc.h"
#include "memlayout.h"

struct {
	struct spinlock lock;
	struct map map[NMAPS];
} maptable;

void mapinit(void){
	initlock(&maptable.lock, "maptable");
}

struct map* mapalloc(void){
	struct map *m;

	acquire(&maptable.lock);
	for (m = maptable.map; m < &maptable.map[NMAPS]; m++){
		if (m->used == 0){
			m->used = 1;
			release(&maptable.lock);
			return m;
		}
	}

	release(&maptable.lock);
	return 0;
}

int unmap(struct map *m){
	uint addr;
	pte_t *pte;
	int write_length;
	int write_suc;
	struct file *f;

	if (m->fd > -1){
		if ((f = myproc()->ofile[m->fd]) ==0){
			return -1;
		}
	} else {
		f = 0;
	}

	cprintf("here\n");

		for (int i = 0; i < m->pages; i++){
			addr = m->addr + 4096*i;
			pte = walkpgdir(myproc()->pgdir, (void*)addr, 0);

			if (f && m->mapshared == 1 && (*pte & PTE_P)){
                        	f->off = 4096*i;
				if (f->ip->size - f->off > 4096){
					write_length = 4096;
				} else {
					write_length = f->ip->size - f->off;
				}

				if ((write_suc = filewrite(f, (char*)addr, write_length)) < 0){
					return -1;
				}
                	}
			if (*pte != 0){
      				char *pa = P2V(PTE_ADDR(*pte));
				if ((m->mapshared == 1 && m->cpid == myproc()->pid) || m->mapshared == 0)
					kfree(pa);
				*pte = 0;
			}
		}

	if ((m->mapshared == 1 && m->cpid == myproc()->pid) || m->mapshared == 0){
		acquire(&maptable.lock);
		m->used = 0;
		m->addr = 0;
		m->pages = 0;
		m->size = 0;
		m->n_alloc_pages = 0;
		m->mapshared = 0;
		m->fd = 0;
		release(&maptable.lock);
	}

	return 0;
}


int ismapped(uint addr){
	struct map *m;
	for (int i = 0; i < MAX_WMAP; i++){
		m = myproc()->wmaps[i];
		if (m != 0 && (addr >= m->addr && addr < m->addr + 4096*(m->pages))){
			return -1;
		}	
	}

	return 0;
}


uint get_addr(int new_addr, int num_pages, int flags){
 uint base = 0x60000000;

 if (flags & MAP_FIXED){
                new_addr = (uint)new_addr;
                if (new_addr % 4096 != 0){
                        return -1;
                } else if (new_addr < base || new_addr >= KERNBASE){
                        return -1;
                }
        } else {
                uint curr_page = base;
                int found = 0;
                while (curr_page < KERNBASE){
                        if (ismapped(curr_page)  == 0){
                                // found an available page staring at new_addr
                                // now check to see if remainig pages are available
                                new_addr = curr_page;
                                int map_available = 1;
                                for (int i = 1; i < num_pages; i++){
                                        // check to make sure all requested pages starting at new_addr are
                                        // available
                                        curr_page = curr_page + 4096;
                                        if (ismapped(curr_page) == -1){
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
                        curr_page = curr_page + 4096;
                }

                if (found == 0){
                        // made it through the whole loop without finding available room
                        // so the request fails
                        return -1;
                }
        }

 	return new_addr;
}
