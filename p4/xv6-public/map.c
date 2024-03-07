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

void unmap(struct map *m){
	uint addr;
	pte_t *pte;
	for (int i = 0; i < m->pages; i++){
		addr = m->addr + 4096*i;
		pte = walkpgdir(myproc()->pgdir, (void*)addr, 0);
		if (*pte != 0){
      			char *pa = P2V(PTE_ADDR(*pte));
			kfree(pa);
			*pte = 0;
		}
	}	

	m->used = 0;
	m->addr = 0;
	m->pages = 0;
	m->n_alloc_pages = 0;
	m->mapshared = 0;
	m->fd = 0;
}
