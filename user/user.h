struct PriorityInfoReport;
struct stat;

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int *);
int pipe(int *);
int write(int, const void *, int);
int read(int, void *, int);
int close(int);
int kill(int);
int exec(const char *, char **);
int open(const char *, int);
int mknod(const char *, short, short);
int unlink(const char *);
int fstat(int fd, struct stat *);
int link(const char *, const char *);
int mkdir(const char *);
int chdir(const char *);
int dup(int);
int getpid(void);
char *sbrk(int);
int sleep(int);
int uptime(void);

// ulib.c
int stat(const char *, struct stat *);
char *strcpy(char *, const char *);
void *memmove(void *, const void *, int);
char *strchr(const char *, char c);
int strcmp(const char *, const char *);
void fprintf(int, const char *, ...);
void printf(const char *, ...);
char *gets(char *, int max);
uint strlen(const char *);
void *memset(void *, int, uint);
void *malloc(uint);
void free(void *);
int atoi(const char *);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);

int pause(int); // my stuff

// my stuff, not from source code (extras...)
// same as that in kernal/proc.h
struct proc_info
{
    int pid;
    int ppid;
    int state;
    uint64 sz;
};

struct resource_usage
{
    int cpuTicks;
    int syscallCount;
    int contextSwitches;
    int sleepCount;
};

int getprocinfo(struct proc_info *info);
int blockchild(int pid);
int unblockchild(int pid);
int getresourceusage(struct resource_usage *usage);

//priority scheduler
int startPriority(int m, int n);
int stopPriority(void);
int getPriorityInfo(struct PriorityInfoReport *report);

