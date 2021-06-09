// This file is a CPU-bound benchmark which forks a number of child processes
// to generate an approximation of e using Monte-Carlo method.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sched.h>
#include <unistd.h>
#include <sys/wait.h>

#define SCHED_FIFO 1
#define SCHED_RR 2
#define SCHED_WRR 6

#define DEFAULT_ITERATIONS 100000
#define DEFAULT_CHILDREN 20
#define MAX_CHILDREN 500

double inline Rand(double L, double R)
{
    return L + (R - L) * rand() * 1.0 / RAND_MAX;
}

double calcE(int iterations)
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

// Parse commadline arguments to get iterations, policy and process number
void parser(int argc, char *argv[], int *iterations, int *policy, int *child_count)
{
    if (argc != 4)
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
}

int main(int argc, char *argv[])
{
    int i;
    int pid;
    int iterations;
    struct sched_param param;
    int policy;
    int child_count;

    // Parse command line
    parser(argc, argv, &iterations, &policy, &child_count);

    // Set process to max priority for given scheduler
    param.sched_priority = sched_get_priority_max(policy);

    // Set new scheduler policy
    if (sched_setscheduler(0, policy, &param))
    {
        fprintf(stderr, "Setting scheduler error!\n");
        exit(1);
    }

    // Start forking children
    int *children = malloc(sizeof(int) * child_count);
    for (i = 0; i < child_count; i++)
    {
        pid = fork();
        if (pid > 0)
        {
            children[i] = pid; 
        }
        else if (pid == 0)
        {
            calcE(iterations);
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