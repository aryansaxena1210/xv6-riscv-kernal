# COMS 3520 – Project 1B

**Name:** Aryan Saxena

---

## Description of Changes

### Task 1 – `getprocinfo`
Added a new system call that retrieves basic information about the calling process. Defined a `struct proc_info` in both kernel and user headers to hold the process ID, parent process ID, process state, and memory size. The kernel handler uses `myproc()` to access the process control block and `copyout()` to safely transfer the data to user space.

### Task 2 – `blockchild` / `unblockchild`
Added two system calls that allow a parent process to explicitly block and unblock its direct child processes. Introduced a new `procstate_extra_t` field (`state_extra`) in `struct proc` with values `BLOCKED` and `UNBLOCKED`. The scheduler was modified to skip any RUNNABLE process whose `state_extra` is `BLOCKED`. Both syscalls verify the parent-child relationship before modifying state, and reject invalid operations such as double-blocking or operating on ZOMBIE/UNUSED processes.

### Task 3 – `getresourceusage`
Added a new system call that returns per-process resource usage counters. Four counters were added to `struct proc`: `cpuTicks` (timer interrupts while running), `syscallCount` (number of system calls made), `contextSwitches` (times scheduled by the scheduler), and `sleepCount` (times voluntarily slept). These are incremented at the appropriate kernel locations and copied to user space by the handler. The call returns the caller's PID on success.

---

## Modified Files

```
kernel/proc.h
kernel/proc.c
kernel/trap.c
kernel/sysproc.c
kernel/syscall.h
kernel/syscall.c
user/user.h
user/usys.pl
user/procmon.c
```


# Project 1C: Priority Scheduler with Aging
COM S 3520 – Spring 2026

## Contributors
- Aryan Saxena

## Description
This project implements a Priority Scheduler as an alternative to
xv6's default Round-Robin (RR) scheduler. The scheduler assigns priorities to
processes and schedules higher priority processes first. To prevent starvation,
an aging mechanism boosts low-priority processes that have waited too long.

The scheduler follows the following rules:
•  Rule 1: Totally m levels of priority are defined: 0, 1, . . . , m − 1. The lower the number, the
higher the priority. Accordingly, m ready queues are maintained.
• Rule 2: When a process enters the system, its starting priority is m/2.
• Rule 3: The scheduler always picks the highest-priority RUNNABLE process. If multiple
processes share the same priority, they are scheduled using round-robin. A process at level x
receives a time quantum of 2(x + 1) ticks.
• Rule 4: Once a process exhausts its time quantum at level x, its priority is degraded to x + 1
if x̸ = m − 1.
• Rule 5 (Aging): After a process waits at level x for n or more ticks, its priority is boosted
to x − 1 unless x = 0.

## Added Files
- `user/testsyscall.c` — test program for the priority scheduler

## Modified Files
- `kernel/proc.h` — added priority scheduler fields to struct proc, defined
  PriorityInfoReport, pq_node structs
- `kernel/proc.c` — implemented Priority_scheduler, RR_scheduler, startPriority,
  stopPriority, getPriorityInfo, priority_enqueue, priority_dequeue, priority_delete
- `kernel/syscall.h` — added system call numbers for startPriority, stopPriority,
  getPriorityInfo
- `kernel/syscall.c` — registered new system calls in syscall table
- `kernel/sysproc.c` — implemented sys_startPriority, sys_stopPriority,
  sys_getPriorityInfo
- `kernel/defs.h` — declared new kernel functions
- `user/usys.pl` — added user-space stubs for new system calls
- `user/user.h` — declared user-space interfaces for new system calls
- `Makefile` — changed CPUS to 1, added testsyscall to UPROGS
