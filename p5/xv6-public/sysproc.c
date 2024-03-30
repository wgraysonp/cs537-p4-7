#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "mutex.h"

// spinlock
struct spinlock mutexlock;
void
initmutexlock(void)
{
  initlock(&mutexlock, "mutexlock");
}

int
sys_fork(void)
{
  return fork();
}

int sys_clone(void)
{
  int fn, stack, arg;
  argint(0, &fn);
  argint(1, &stack);
  argint(2, &arg);
  return clone((void (*)(void*))fn, (void*)stack, (void*)arg);
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  if (n == 0) {
    yield();
    return 0;
  }
  acquire(&tickslock);
  ticks0 = ticks;
  myproc()->sleepticks = n;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  myproc()->sleepticks = -1;
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// new system calls

int sys_nice(void)
{
	int inc;

	if (argint(0, &inc) < 0)
		return -1;

	if (inc > 19){
		inc = 19;
	} else if (inc < -20){
		inc = -20;
	}
	myproc()->nice = inc;
	return 0;
}


int
sys_macquire(void)
{
  mutex *m;
  if(argptr(0, (char**)&m, sizeof(*m)) < 0) {
    return -1; // mutex was not retrieved
  }

  acquire(&mutexlock);
  while (m->locked) {
    sleep(m, &mutexlock);
  }
  m->locked = 1;
  m->pid = myproc()->pid;
  release(&mutexlock);

  return 0;
}

int 
sys_mrelease(void)
{
  mutex *m;
  if(argptr(0, (char**)&m, sizeof(*m)) < 0) {
    return -1; // mutex was not retrieved
  } 
  acquire(&mutexlock);
  m->locked = 0;
  m->pid = 0;
  wakeup(m);
  release(&mutexlock);

  return 0;
}


