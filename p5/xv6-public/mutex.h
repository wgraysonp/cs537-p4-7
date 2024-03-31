#include "types.h"

typedef struct {
  uint locked; 
//struct spinlock lock; // spinlock protecting this sleep lock
//  char *name; // name of lock
  int pid; // process holding lock
} mutex;

void elevpriority(mutex *m);

