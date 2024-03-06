#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "wmap.h"

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
