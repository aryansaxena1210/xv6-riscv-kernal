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
