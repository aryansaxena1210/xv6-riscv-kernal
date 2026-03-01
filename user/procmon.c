
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/*
 * procmon.c
 *
 * This program tests:
 *   - getprocinfo()
 *   - blockchild()
 *   - unblockchild()
 *   - getresourceusage()
 *
 * The parent process runs for an extended period and
 * performs repeated system calls so that resource
 * usage counters become meaningful.
 */

int
main(void)
{
  struct proc_info info;
  struct resource_usage usage;

  printf("=== Testing getprocinfo ===\n");

  if (getprocinfo(&info) < 0) {
    printf("getprocinfo failed\n");
    exit(1);
  }

  printf("Parent: pid=%d ppid=%d state=%d mem=%lu\n",
         info.pid, info.ppid, info.state, info.sz);

  printf("\n=== Forking child process ===\n");

  int child = fork();
  if (child < 0) {
    printf("fork failed\n");
    exit(1);
  }

  if (child == 0) {
    /*
     * Child process:
     * Prints periodically so it is obvious when
     * it is running versus when it is blocked.
     */
    while (1) {
      printf("Child (%d): running...\n", getpid());
      pause(5);
    }
  }

  /* Parent continues */
  pause(2);  // allow child to start running

  printf("\nBlocking child pid=%d\n", child);
  if (blockchild(child) < 0) {
    printf("blockchild failed\n");
  } else {
    printf("Child blocked (should stop printing)\n");
  }

  pause(10);  // child should not run during this time

  printf("\nUnblocking child pid=%d\n", child);
  if (unblockchild(child) < 0) {
    printf("unblockchild failed\n");
  } else {
    printf("Child unblocked (should resume printing)\n");
  }

  /*
   * Parent workload: repeatedly performs system calls
   * to generate resource usage statistics.
   */
  printf("\n=== Parent running workload ===\n");

  for (int i = 0; i < 20; i++) {
    printf("Parent loop %d, pid=%d, uptime=%d\n",
           i, getpid(), uptime());
    pause(3);
    double x=987654321.9;
    for(int j=0; j<1000000000; j++){
      x/=12345.6789;
    }
  }

  printf("\n=== Testing getresourceusage ===\n");

  int pid = getresourceusage(&usage);
  if (pid < 0) {
    printf("getresourceusage failed\n");
    exit(1);
  }

  printf("Resource usage for pid=%d:\n", pid);
  printf("  cpuTicks        = %d\n", usage.cpuTicks);
  printf("  syscallCount    = %d\n", usage.syscallCount);
  printf("  contextSwitches = %d\n", usage.contextSwitches);
  printf("  sleepCount      = %d\n", usage.sleepCount);

  printf("\nKilling child and exiting\n");
  kill(child);
  wait(0);

  exit(0);
}
