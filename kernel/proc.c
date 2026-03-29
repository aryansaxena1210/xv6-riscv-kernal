#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

//forward declaration
void priority_enqueue(int level, struct proc *p);
void priority_dequeue(int level);
void priority_delete(int level, struct proc *p);
// stuff form me -  Priority Scheduler globals
int priorityFlag = 0;                          // 1 if priority scheduler is active
int M = 0;                                     // number of priority levels
int N = 0;                                     // aging threshold in ticks
struct pq_node *queues[PRIORITY_MAX_LEVEL];    // head of each priority level's linked list

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    char *pa = kalloc();
    if (pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int)(p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void procinit(void)
{
  struct proc *p;

  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    initlock(&p->lock, "proc");
    p->state = UNUSED;
    p->kstack = KSTACK((int)(p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc *
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int allocpid()
{
  int pid;

  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->state == UNUSED)
    {
      goto found;
    }
    else
    {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if ((p->trapframe = (struct trapframe *)kalloc()) == 0)
  {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if (p->pagetable == 0)
  {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if (p->trapframe)
    kfree((void *)p->trapframe);
  p->trapframe = 0;
  if (p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;

  // my stuff, not from source code (extras...)
  p->state_extra = UNBLOCKED;

  p->cpuTicks = 0;
  p->syscallCount = 0;
  p->contextSwitches = 0;
  p->sleepCount = 0;

  //priority scheduler 
  p->in_priority_queue = 0;
  p->priority_level = 0;
  p->ticks_run = 0;
  p->ticks_waited = 0;
  for (int i = 0; i < PRIORITY_MAX_LEVEL; i++)
    p->tickCounts[i] = 0;

}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if (pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if (mappages(pagetable, TRAMPOLINE, PGSIZE,
               (uint64)trampoline, PTE_R | PTE_X) < 0)
  {
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if (mappages(pagetable, TRAPFRAME, PGSIZE,
               (uint64)(p->trapframe), PTE_R | PTE_W) < 0)
  {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
    0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
    0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
    0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
    0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
    0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
    0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

// Set up first user process.
void userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;

  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;     // user program counter
  p->trapframe->sp = PGSIZE; // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if (n > 0)
  {
    if ((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0)
    {
      return -1;
    }
  }
  else if (n < 0)
  {
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy user memory from parent to child.
  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0)
  {
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void reparent(struct proc *p)
{
  struct proc *pp;

  for (pp = proc; pp < &proc[NPROC]; pp++)
  {
    if (pp->parent == p)
    {
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void exit(int status)
{
  struct proc *p = myproc();

  if (p == initproc)
    panic("init exiting");

  // Close all open files.
  for (int fd = 0; fd < NOFILE; fd++)
  {
    if (p->ofile[fd])
    {
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);

  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (pp = proc; pp < &proc[NPROC]; pp++)
    {
      if (pp->parent == p)
      {
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if (pp->state == ZOMBIE)
        {
          // Found one.
          pid = pp->pid;
          if (addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                   sizeof(pp->xstate)) < 0)
          {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || killed(p))
    {
      release(&wait_lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep(p, &wait_lock); // DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
RR_scheduler(struct cpu *c)
{
  struct proc *p;
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == RUNNABLE) {
      p->state = RUNNING;
      c->proc = p;
      swtch(&c->context, &p->context);
      c->proc = 0;
    }
    release(&p->lock);
  }
}


// my stff - Priority-based scheduler with aging 
// NOTE - Tick Accounting Fix:
// Originally, the ticks were incremented at the top of the scheduler loop. This is
// incorrect since the while loop simply runs freely and is not related to the hardware
// timer. In xv6, one tick = one hardware timer interrupt. The timer interrupts,
// which then call usertrap() -> yield() -> sched() -> swtch() back to the
// scheduler. Therefore, swtch() returning is the exact point where we know one tick
// has passed. As a result, ticks_run and tickCounts are now incremented immediately
// after swtch() returns.
void
Priority_scheduler(struct cpu *c)
{
  struct proc *p = 0;

  while (priorityFlag) {

    // Step 1: Add any RUNNABLE processes not yet in a queue
    // Rule 2: new processes start at M/2
    for (struct proc *np = proc; np < &proc[NPROC]; np++) {
      acquire(&np->lock);
      if (np->state == RUNNABLE && !np->in_priority_queue) {
        priority_enqueue(M / 2, np);
      }
      release(&np->lock);
    }

    // Step 2: Select highest-priority RUNNABLE process (if none selected)
    if (p == 0) {
      for (int i = 0; i < M; i++) {
        struct pq_node *node = queues[i];
        while (node != 0) {
          if (node->p->state == RUNNABLE) {
            p = node->p;
            break;
          }
          node = node->next;
        }
        if (p != 0) break;
      }
    }

    // Step 3: Run the selected process for one tick
    if (p != 0 && p->state == RUNNABLE) {
      acquire(&p->lock);
      p->state = RUNNING;
      c->proc = p;
      swtch(&c->context, &p->context);
      // ONE TICK HAS PASSED — timer fired, process yielded back
      p->ticks_run++;
      p->tickCounts[p->priority_level]++;
      c->proc = 0;
      release(&p->lock);
    }

    // Step 4: Check if process used up its quantum
    // Rule 3: quantum = 2*(level+1) ticks
    // Rule 4: degrade priority if quantum exhausted
    if (p != 0) {
      if (p->state != RUNNABLE) {
        // process went to sleep or exited, remove from queue
        if (p->in_priority_queue)
          priority_delete(p->priority_level, p);
        p = 0;
      } else {
        int level = p->priority_level;
        int quantum = 2 * (level + 1);
        if (p->ticks_run >= quantum) {
          priority_delete(level, p);
          p->ticks_run = 0;
          int next_level = (level < M - 1) ? level + 1 : level;
          priority_enqueue(next_level, p);
          p = 0;  // reselect next iteration
        }
      }
    }

    // Step 5: Aging — Rule 5
    // Boost processes that have waited >= N ticks at level x to x-1
    for (int i = 1; i < M; i++) {
      struct pq_node *node = queues[i];
      while (node != 0) {
        struct pq_node *next = node->next;
        if (node->p != p) {  // don't age the currently running process
          node->p->ticks_waited++;
          if (node->p->ticks_waited >= N) {
            struct proc *boosted = node->p;
            priority_delete(i, boosted);
            boosted->ticks_run = 0;
            priority_enqueue(i - 1, boosted);
          }
        }
        node = next;
      }
    }

  } // end while(priorityFlag)
}


// Main scheduler — dispatches to priority or RR based on priorityFlag.
void
scheduler(void)
{
  struct cpu *c = mycpu();
  c->proc = 0;
  for (;;) {
    intr_on();
    if (priorityFlag) {
      Priority_scheduler(c);
    } else {
      RR_scheduler(c);
    } 
  }
}


// Starts the priority scheduler with M levels and aging threshold N.
void
startPriority(int m, int n)
{
  M = m;
  N = n;
  // Initialize all queues to empty
  for (int i = 0; i < M; i++)
    queues[i] = 0;
  priorityFlag = 1;
}

// Stops the priority scheduler and resumes RR.
void
stopPriority(void)
{
  priorityFlag = 0;
}

// Fills in report with tick counts for the calling process.
int
getPriorityInfo(uint64 addr)
{
  struct proc *p = myproc();
  struct PriorityInfoReport report;
  for (int i = 0; i < PRIORITY_MAX_LEVEL; i++)
    report.tickCounts[i] = p->tickCounts[i];
  if (copyout(p->pagetable, addr, (char *)&report, sizeof(report)) < 0)
    return -1;
  return 0;
}




// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&p->lock))
    panic("sched p->lock");
  if (mycpu()->noff != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first)
  {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock); // DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;
  p->sleepCount++;
  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void wakeup(void *chan)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p != myproc())
    {
      acquire(&p->lock);
      if (p->state == SLEEPING && p->chan == chan)
      {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int pid)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->pid == pid)
    {
      p->killed = 1;
      if (p->state == SLEEPING)
      {
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int killed(struct proc *p)
{
  int k;

  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if (user_dst)
  {
    return copyout(p->pagetable, dst, src, len);
  }
  else
  {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if (user_src)
  {
    return copyin(p->pagetable, dst, src, len);
  }
  else
  {
    memmove(dst, (char *)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [USED] "used",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  struct proc *p;
  char *state;

  printf("\n");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

// Helper function: find a proc by pid that is a direct child of 'parent'
static struct proc *
find_child(struct proc *parent, int pid)
{
  struct proc *p;
  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p->pid == pid && p->parent == parent)
      return p;
  }
  return 0;
}

uint64
sys_blockchild(void)
{
  int pid;
  argint(0, &pid);

  struct proc *caller = myproc();
  struct proc *child = find_child(caller, pid);

  if (child == 0)
    return -1; // either not a direct child or just not found at all

  // do NOT block the Zombie or Unused
  if (child->state == ZOMBIE || child->state == UNUSED)
    return -1;

  if (child->state_extra == BLOCKED)
    return -1; // blocked already

  child->state_extra = BLOCKED;
  return 0;
}

uint64
sys_unblockchild(void)
{
  int pid;
  argint(0, &pid);

  struct proc *caller = myproc();
  struct proc *child = find_child(caller, pid);

  if (child == 0)
    return -1;

  if (child->state == ZOMBIE || child->state == UNUSED)
    return -1;
 
  if (child->state_extra != BLOCKED)
    return -1; // not currently blocked

  child->state_extra = UNBLOCKED;
  return 0;
}


//stuff form me - priority scheduler helpers 
void
priority_enqueue(int level, struct proc *p)
{
  struct pq_node *node = (struct pq_node *)kalloc();
  node->p = p;
  node->prev = 0;
  node->next = queues[level];
  if (queues[level] != 0)
    queues[level]->prev = node;
  queues[level] = node;
  p->in_priority_queue = 1;
  p->priority_level = level;
  p->ticks_waited = 0;
}

void
priority_dequeue(int level)
{
  if (queues[level] == 0)
    return;
  struct pq_node *node = queues[level];
  queues[level] = node->next;
  if (queues[level] != 0)
    queues[level]->prev = 0;
  node->p->in_priority_queue = 0;
  kfree((void *)node);
}

void
priority_delete(int level, struct proc *p)
{
  struct pq_node *node = queues[level];
  while (node != 0) {
    if (node->p == p) {
      if (node->prev != 0)
        node->prev->next = node->next;
      else
        queues[level] = node->next;
      if (node->next != 0)
        node->next->prev = node->prev;
      p->in_priority_queue = 0;
      kfree((void *)node);
      return;
    }
    node = node->next;
  }
}



//stuff from me - priority queue scheduler