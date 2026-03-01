#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAXLINE 128
#define MAXWORKERS 10

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

int strncmp(const char *p, const char *q, int n)
{
    while (n > 0 && *p == *q)
    {
        if (*p == 0)
            return 0;
        p++;
        q++;
        n--;
    }

    if (n == 0)
        return 0;

    return (unsigned char)*p - (unsigned char)*q;
}

/*
 * Worker:
 *  - read a message from parent
 *  - append its worker id
 *  - send back to parent
 *  - exit on :exit or :EXIT
 */
void worker(int id, int read_fd, int write_fd)
{
    char buf[MAXLINE];

    while (1)
    {
        memset(buf, 0, sizeof(buf));

        if (read(read_fd, buf, sizeof(buf)) <= 0)
            break;

        if (strcmp(buf, ":exit") == 0 || strcmp(buf, ":EXIT") == 0)
            break;

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
        fprintf(2, "Usage: relay_plus <num_workers>\n");
        exit(1);
    }

    int n = atoi(argv[1]);

    if (n <= 0 || n > MAXWORKERS)
    {
        fprintf(2, "Number of workers must be between 1 and %d\n", MAXWORKERS);
        exit(1);
    }

    int p2w[MAXWORKERS][2];
    int w2p[MAXWORKERS][2];

    for (int i = 0; i < n; i++)
    {
        pipe(p2w[i]);
        pipe(w2p[i]);
    }

    for (int i = 0; i < n; i++)
    {
        int pid = fork();
        if (pid == 0)
        {

            for (int j = 0; j < n; j++)
            {
                if (j != i)
                {
                    close(p2w[j][0]);
                    close(p2w[j][1]);
                    close(w2p[j][0]);
                    close(w2p[j][1]);
                }
            }

            close(p2w[i][1]); // parent write end
            close(w2p[i][0]); // parent read end

            worker(i + 1, p2w[i][0], w2p[i][1]);
        }
    }

    for (int i = 0; i < n; i++)
    {
        close(p2w[i][0]);
        close(w2p[i][1]);
    }

    char cmd[MAXLINE];
    char mode[MAXLINE];

    while (1)
    {
        printf("Enter command: ");
        memset(cmd, 0, sizeof(cmd));
        // gets(cmd, sizeof(cmd));
        if (gets(cmd, sizeof(cmd)) == 0)
            break;
        strip_newline(cmd);

        printf("Mode (:all | :first k | :skip k): ");
        memset(mode, 0, sizeof(mode));
        // gets(mode, sizeof(mode));
        if (gets(mode, sizeof(mode)) == 0)
            break;
        strip_newline(mode);

        if (strcmp(cmd, ":exit") == 0 || strcmp(cmd, ":EXIT") == 0)
        {

            for (int i = 0; i < n; i++)
                write(p2w[i][1], cmd, strlen(cmd) + 1);

            break;
        }

        int use[MAXWORKERS];
        for (int i = 0; i < n; i++)
            use[i] = 0;

        int valid = 1;

        if (strcmp(mode, ":all") == 0)
        {

            for (int i = 0; i < n; i++)
                use[i] = 1;
        }
        else if (strncmp(mode, ":first ", 7) == 0)
        {

            int k = atoi(mode + 7);

            if (k < 1 || k > n)
            {
                printf("Error: invalid number of workers\n");
                valid = 0;
            }
            else
            {
                for (int i = 0; i < k; i++)
                    use[i] = 1;
            }
        }
        else if (strncmp(mode, ":skip ", 6) == 0)
        {

            int k = atoi(mode + 6);

            if (k < 1 || k > n)
            {
                printf("Error: worker %d does not exist\n", k);
                valid = 0;
            }
            else
            {
                for (int i = 0; i < n; i++)
                    use[i] = 1;
                use[k - 1] = 0;
            }
        }
        else
        {
            printf("Error: malformed mode\n");
            valid = 0;
        }

        if (!valid)
            continue;

        char buf[MAXLINE];
        memset(buf, 0, sizeof(buf));
        strcpy(buf, cmd);

        for (int i = 0; i < n; i++)
        {
            if (!use[i])
                continue;

            write(p2w[i][1], buf, strlen(buf) + 1);

            memset(buf, 0, sizeof(buf));
            read(w2p[i][0], buf, sizeof(buf));
        }

        printf("Result: %s\n", buf);
    }

    for (int i = 0; i < n; i++)
        wait(0);

    for (int i = 0; i < n; i++)
    {
        close(p2w[i][1]);
        close(w2p[i][0]);
    }

    exit(0);
}
