#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "wmap.h"
#include "fs.h"
#include "file.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  case T_PGFLT:
    	struct map *m;
	char *mem;	
	int found = 0;
	int map_suc;
	int file_suc;
	struct file *f;
	for (int i = 0; i < MAX_WMAP; i++){
		m = myproc()->wmaps[i];
		if (m->addr <= rcr2() && m->addr + 4096*(m->pages) > rcr2()){
			uint pagebase;
			if (m->fd > -1){
				for (int i = 0; i < m->pages; i++){
					if ((mem = kalloc()) == 0){
						cprintf("Allocation failed\n");
						myproc()->killed = 1;
						exit();
					}
					pagebase = m->addr + 4096*i;
					if ((map_suc = mappages(myproc()->pgdir, (void*)pagebase, 4096, V2P(mem), PTE_W | PTE_U))==-1){
						cprintf("mappages failed\n");
						myproc()->killed = 1;
						exit();
					}
					m->n_alloc_pages++;
				}


				if((f = myproc()->ofile[m->fd]) == 0){
					// this should already be checked for in sysmap.c
					// but no harm in checking twice
					cprintf("File not open");
					myproc()->killed = 1;
					exit();
				}

				if ((file_suc = fileread(f, (char*)(m->addr), m->size)) < 0){
				        cprintf("File read failed\n");
			       	       	myproc()->killed = 1;
				 	exit();
				}
	

			} else {
				pagebase = (rcr2()/4096)*4096;
				mem = kalloc();
				if (mem == 0){
					cprintf("Allocation failed\n");
					myproc()->killed = 1;
					exit();
				}
				if ((map_suc =  mappages(myproc()->pgdir, (void*)pagebase, 4096, V2P(mem), PTE_W | PTE_U)) == -1){
					cprintf("mappages failed\n");
					myproc()->killed = 1;
					exit();
				}
				m->n_alloc_pages++;
			}


			found = 1;
			break;
		}

	}
	if (found == 0){
		// mapping doesnt exist
		cprintf("Segmentation Fault addr 0x%x\n", rcr2());
		myproc()->killed=1;
		exit();
	}
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
