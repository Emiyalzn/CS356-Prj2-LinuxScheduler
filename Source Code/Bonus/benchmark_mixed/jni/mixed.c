// This file is a mixed benchmark which forks a number of child processes
// to do computation and IO operations.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sched.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>

#define SCHED_FIFO 1
#define SCHED_RR 2
#define SCHED_WRR 6

#define DEFAULT_ITERATIONS 100000
#define MAX_CHILDREN 500
#define MAX_FILENAME_LEN 100
#define CHILD_ITERATIONS 100

double inline Rand(double L, double R)
{
    return L + (R - L) * rand() * 1.0 / RAND_MAX;
}

double CPU(int iterations)
{
    int m = 0;
    int n = iterations;
    double x, y;

    // Initialize random seeds
    srand(time(NULL));

    // Calculate e using Monte-Carlo method across all iterations
    for(int i = 0; i < iterations; i++)
    {
        x = Rand(1.0, 2.0);
        y = Rand(0.0,1.0);
        double res = x * y;
        if(res <= 1.0)
            m++;
    }

    // Calculate e based on probability
    return pow(2.0, 1.0 * n / m);
}

void IO(size_t block_size, size_t transfer_size, char* src_name, char* dest_name) 
{
    int srcFile;
    int destFile;
    ssize_t read_bytes = 0;
    ssize_t write_bytes = 0;
    ssize_t tot_bytes = 0;
    char* buf = NULL;

    // malloc buffer
    buf = malloc(block_size * sizeof(char));
    if (buf == NULL) 
    {
        fprintf(stderr, "Buffer allocating error!\n");
        exit(1);
    }

    // Open source file
    srcFile = open(src_name, O_RDONLY);
    if (srcFile < 0) 
    {
        fprintf(stderr, "Couldn't open source file!\n");
        exit(1);
    }

    // Open/create dest file
    destFile = open(dest_name, O_WRONLY | O_CREAT);
    if (destFile < 0) 
    {
        fprintf(stderr, "Couldn't open destination file!\n");
        exit(1);
    }

    // Copy source file into destination file
    while (tot_bytes < (ssize_t)transfer_size)
    {
        read_bytes = read(srcFile, buf, block_size);
        if (read_bytes < 0)
        {
            fprintf(stderr, "Couldn't read source file!\n");
            exit(1);
        }

        if (read_bytes > 0) 
        {
            write_bytes = write(destFile, buf, read_bytes);
            if (write_bytes < 0)
            {
                fprintf(stderr, "Couldn't write to destination file!\n");
                exit(1);
            } 
            else 
            {
                tot_bytes += write_bytes;
            }
        }
    }

    if (close(srcFile))
    {
        fprintf(stderr, "Closing source file error!\n");
        exit(1);
    }
    if (close(destFile))
    {
        fprintf(stderr, "Closing destination file error!\n.");
        exit(1);
    }

    free(buf);
}

// Child process should do computation and IO repeatedly
void childTask(long iterations, char* src, char* dest, size_t b_size, size_t t_size)
{
    for(int i=0; i < CHILD_ITERATIONS; ++i)
    {
        CPU(iterations / CHILD_ITERATIONS);
        IO(b_size, t_size / CHILD_ITERATIONS, src, dest);
    }
}

void parser(int argc, char* argv[], long* iterations, int* policy, int* child_count, 
                    size_t* block_size, ssize_t* transfer_size, char* src_name, char* dest_name)
{
    if (argc != 8)
    {
        fprintf(stderr, "Input format error!\n");
        exit(1);
    }

    // Set iterations
    *iterations = atoi(argv[1]);
    if (*iterations < 1)
    {
        fprintf(stderr, "Iterations out of range!\n");
        exit(1);
    }

    // Set policy
    if (!strcmp(argv[2], "SCHED_WRR"))
    {
        *policy = SCHED_WRR;
    }
    else if (!strcmp(argv[2], "SCHED_FIFO"))
    {
        *policy = SCHED_FIFO;
    }
    else if (!strcmp(argv[2], "SCHED_RR"))
    {
        *policy = SCHED_RR;
    }
    else
    {
        fprintf(stderr, "Undefined scheduling policy!\n");
        exit(1);
    }

    // Set child number
    *child_count = atoi(argv[3]);
    if (*child_count < 1 || *child_count > MAX_CHILDREN)
    {
        fprintf(stderr, "Children number error!\n");
        exit(1);
    }

    // Set block size
    if (atoi(argv[4]) < 0)
    {
        fprintf(stderr, "Invalid block size.\n");
        exit(1);
    }
    else{
        *block_size = atoi(argv[4]);
    }

    // Set transfer size
    if (atoi(argv[5]) < 0 || atoi(argv[5]) < (int)(*block_size))
    {
        fprintf(stderr, "Invalid transfer size.\n");
        exit(1);
    } 
    else 
    {
        *transfer_size = atoi(argv[5]);
    }

     // Set source file
    if (strlen(argv[6]) > MAX_FILENAME_LEN)
    {
        fprintf(stderr, "File name length out of range!\n");
    }
    else{
        strcpy(src_name, argv[6]);
    }

    // Set dest file
    if (strlen(argv[7]) > MAX_FILENAME_LEN)
    {
        fprintf(stderr, "File name length out of range!\n");
    }
    else{
        strcpy(dest_name, argv[7]);
    }
}

int main(int argc, char* argv[]){
    int i;
    int pid;
    long iterations;
    struct sched_param param;
    int policy;
    int child_count;
    size_t block_size = 0;
    ssize_t transfer_size = 0;
    char src_name[MAX_FILENAME_LEN];
    char dest_name[MAX_FILENAME_LEN];

    srand(time(NULL));

    // Parse command line
    parser(argc, argv, &iterations, &policy, &child_count, &block_size, &transfer_size, src_name, dest_name);

    // Set process to max priority for given scheduler
    param.sched_priority = sched_get_priority_max(policy);

    // Set new scheduler policy
    if (sched_setscheduler(0, policy, &param))
    {
        fprintf(stderr, "Error setting scheduler policy\n");
        exit(1);
    }

    // Start forking children
    int* children = malloc(sizeof(int) * child_count);
    for (i = 0; i < child_count; i++)
    {
        pid = fork();
        if (pid > 0)
        {
            children[i] = pid;
        }
        else if (pid == 0)
        {
            childTask(iterations, argv[6], argv[7], block_size, transfer_size);
            exit(0);
        }
        else if (pid < 0)
        {
            fprintf(stderr, "Error forking.\n");
            exit(1);
        }
    }

    for (i = 0; i < child_count; i++)
    {
        waitpid(children[i], NULL, 0);
    }
    free(children);

    return 0;
}