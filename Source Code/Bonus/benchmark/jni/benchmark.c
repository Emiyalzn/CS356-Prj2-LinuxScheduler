// This file is a driver program written to test three kinds of benchmarks
// under a sequence of processes with different scheduling policies and 
// print out basic informations.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCHED_FIFO 1
#define SCHED_RR 2
#define SCHED_WRR 6

#define cpu_min 20
#define cpu_max 80
#define cpu_step 5

#define io_min 20
#define io_max 80
#define io_step 5

#define mixed_min 20
#define mixed_max 80
#define mixed_step 5

FILE *fp;
char command[1024];
char sched[1024];

void get_scheduler(int policy)
{
    switch (policy)
    {
        case 1:
            strcpy(sched, "SCHED_FIFO");
            break;
        case 2:
            strcpy(sched, "SCHED_RR");
            break;
        case 3:
            strcpy(sched, "SCHED_WRR");
            break;
    }
}

void CPUbound(int policy, FILE *fp)
{ // to get the running time of test_cpubound
    memset(sched, 0, sizeof(sched));
    get_scheduler(policy);

    for(int i = cpu_min; i <= cpu_max; i += cpu_step)
    {
        memset(command, 0, sizeof(command));

        printf("PROC_COUNT = %d\n", i);
        sprintf(command, "time -p ./test_cpubound 100000 %s %d", sched, i);
        fp = popen(command, "r");
        pclose(fp);
    }
}

void IObound(int policy, FILE *fp)
{ // to get the running time of test_iobound
    memset(sched, 0, sizeof(sched));
    get_scheduler(policy);

    for (int i = io_min; i <= io_max; i += io_step)
    {
    	memset(command, 0, sizeof(command));

    	printf("PROC_COUNT = %d\n", i);
        sprintf(command, "time -p ./test_iobound %s /data/misc/data_in /data/misc/data_out 2000 5000000 %d", ret, i);
        fp = popen(command, "r");
        pclose(fp);
    }
}

void Mixed(int policy, FILE *fp)
{ // to get the running time of test_mixed
    memset(sched, 0, sizeof(sched));
    get_scheduler(policy);

    for (int i = mixed_min; i <= mixed_max; i += mixed_step)
    {
    	memset(command, 0, sizeof(command));

    	printf("PROC_COUNT = %d\n", i);
        sprintf(command, "time -p ./test_mixed 100000 %s %d 2000 5000000 /data/misc/data_in /data/misc/data_out2", ret, i);
        fp = popen(command, "r");
        pclose(fp);
    }
}

void cpu_res()
{
    printf("The CPU Bound Results!\n");
    printf("SCHED_FIFO:\n");
    CPUbound(SCHED_FIFO, fp);
    printf("SCHED_RR:\n");
    CPUbound(SCHED_RR, fp);
    printf("SCHED_WRR:\n");
    CPUbound(SCHED_WRR, fp);
}

void io_res()
{
    printf("The IO Bound Results!\n");
	printf("SCHED_FIFO:\n");
    IObound(SCHED_FIFO, fp);
    printf("SCHED_RR:\n");
    IObound(SCHED_RR, fp);
    printf("SCHED_WRR:\n");
    IObound(SCHED_WRR, fp);
}

void mixed_res()
{
    printf("The Mixed Results!\n");
	printf("SCHED_FIFO:\n");
    Mixed(SCHED_FIFO, fp);
    printf("SCHED_RR:\n");
    Mixed(SCHED_RR, fp);
    printf("SCHED_WRR:\n");
    Mixed(SCHED_WRR, fp);
}

int main()
{
        cpu_res();

        io_res();

        mixed_res();
}