// relay_sample.c
//
// A simple multi-process command relay for xv6-riscv.
// One parent process and N worker processes connected via pipes.
//
// TA NOTE:
// This file is PROVIDED to students as a reading example.
// Students are NOT graded on modifying this file.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAXLINE 128
#define MAXWORKERS 3

void strip_newline(char *s)
{
  int i = 0;
  while (s[i])
  {
    if (s[i] == '\n')
    {
      s[i] = '\0';
      return;
    }
    i++;
  }
}

void worker(int id, int read_fd, int write_fd)
{
  char buf[MAXLINE];

  while (1)
  {
    memset(buf, 0, sizeof(buf));

    if (read(read_fd, buf, sizeof(buf)) <= 0)
    {
      break;
    }

    if (strcmp(buf, ":quit") == 0)
    {
      write(write_fd, buf, strlen(buf) + 1);
      break;
    }

    // Append worker ID
    char out[MAXLINE];
    memset(out, 0, sizeof(out));
    strcpy(out, buf);

    char suffix[8];
    memset(suffix, 0, sizeof(suffix));
    suffix[0] = ' ';
    suffix[1] = '[';
    suffix[2] = 'W';
    suffix[3] = '0' + id; // assumes id < 10
    suffix[4] = ']';
    suffix[5] = '\0';

    int len = strlen(out);
    strcpy(out + len, suffix);

    write(write_fd, out, strlen(out) + 1);
  }

  close(read_fd);
  close(write_fd);
  exit(0);
}

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    fprintf(2, "Usage: relay_sample <num_workers>\n");
    exit(1);
  }

  int n = atoi(argv[1]);
  if (n <= 0 || n > MAXWORKERS)
  {
    fprintf(2, "Number of workers must be between 1 and %d\n", MAXWORKERS);
    exit(1);
  }

  int pipes[MAXWORKERS + 1][2];

  for (int i = 0; i <= n; i++)
  {
    pipe(pipes[i]);
  }

  for (int i = 1; i <= n; i++)
  {
    int pid = fork();
    if (pid == 0)
    {
      // Worker process
      for (int j = 0; j <= n; j++)
      {
        if (j != i - 1)
          close(pipes[j][0]);
        if (j != i)
          close(pipes[j][1]);
      }

      worker(i, pipes[i - 1][0], pipes[i][1]);
    }
  }

  // Parent
  for (int i = 0; i <= n; i++)
  {
    if (i != n)
      close(pipes[i][0]);
    if (i != 0)
      close(pipes[i][1]);
  }

  char buf[MAXLINE];

  while (1)
  {
    printf("Enter command: ");
    memset(buf, 0, sizeof(buf));
    gets(buf, sizeof(buf));
    strip_newline(buf);

    write(pipes[0][1], buf, strlen(buf) + 1);

    if (strcmp(buf, ":quit") == 0)
      break;

    memset(buf, 0, sizeof(buf));
    read(pipes[n][0], buf, sizeof(buf));

    printf("Result: %s\n", buf);
  }

  for (int i = 0; i < n; i++)
    wait(0);

  close(pipes[0][1]);
  close(pipes[n][0]);

  exit(0);
}
