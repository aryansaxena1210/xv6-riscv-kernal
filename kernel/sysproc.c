#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (killed(myproc()))
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_getprocinfo(void)
{
  struct proc *p = myproc();
  struct proc_info info;
  uint64 uaddr;

  // Populate from the process control block
  info.pid = p->pid;
  info.ppid = p->parent ? p->parent->pid : 0;
  info.state = p->state;
  info.sz = p->sz;

  argaddr(0, &uaddr);

  if (copyout(p->pagetable, uaddr, (char *)&info, sizeof(info)) < 0)
    return -1;
  return 0;
}

uint64 sys_getresourceusage(void)
{
  struct proc *p = myproc();
  struct resource_usage usage;
  uint64 uaddr;

  usage.cpuTicks = p->cpuTicks;
  usage.syscallCount = p->syscallCount;
  usage.contextSwitches = p->contextSwitches;
  usage.sleepCount = p->sleepCount;

  argaddr(0, &uaddr);

  if (copyout(p->pagetable, uaddr, (char *)&usage, sizeof(usage)) < 0)
    return -1;

  return p->pid; // returns PID on success per spec
}



// Starts the priority scheduler with m levels and aging threshold n.
uint64
sys_startPriority(void)
{
  int m, n;
  argint(0, &m);
  argint(1, &n);
  startPriority(m, n);
  return 0;
}

// Stops the priority scheduler and resumes RR.
uint64
sys_stopPriority(void)
{
  stopPriority();
  return 0;
}

// Fills in the report with tick counts at each priority level.
uint64
sys_getPriorityInfo(void)
{
  uint64 addr;
  argaddr(0, &addr);
  return getPriorityInfo(addr);
}
