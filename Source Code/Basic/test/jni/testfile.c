/*
 * This file is created to test the new WRR scheduler
 * It also prints basic information of processtest.apk using syscalls
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>

#define MAX_LEN 100

//Get intended scheduler name
void getSched(char* sched, int number)
{
        switch (number)
        {
                case 0:
                        strcpy(sched, "SCHED_NORMAL");
                        break;
                case 1:
                        strcpy(sched, "SCHED_FIFO");
                        break;
                case 2:
                        strcpy(sched, "SCHED_RR");
                        break;
                case 6:
                        strcpy(sched, "SCHED_WRR");
                        break;
        }
}

//Print basic process information
int printInfo(char *info)
{
        char *p;
    char group[MAX_LEN];

        strtok(info, " ");
        p = strtok(NULL, " ");
        int pid = atoi(p);
        printf("PID: %d\n", pid);

    strtok(NULL, " "); strtok(NULL, " "); strtok(NULL, " ");
        p = strtok(NULL, " ");
        if (p[0] == 'f') strcpy(group, "Foreground");
        else if (p[0] == 'b') strcpy(group, "Background");
        printf("Group: %s\n", group);

    strtok(NULL, " "); strtok(NULL, " "); strtok(NULL, " ");
        p = strtok(NULL, " ");
        printf("Process Name: %s\n", p);

        return pid;
}

int main(){
    // find the process and store its information
    FILE *fp = popen("ps -P|grep processtest","r");
    char info[MAX_LEN] = {'\0'};
    while(fgets(info, MAX_LEN, fp) != NULL);
    pclose(fp);                                        
    printf("///////////////////////info@processtest////////////////////\n");

        int pid = printInfo(info);

    // change the scheduler
        int intended_sched, prio;
        printf("Scheduling Algorithm List:\nSCHED_NORMAL\t0\nSCHED_FIFO\t1\nSCHED_RR\t2\nSCHED_WRR\t6\n\n");

        printf("Please choose the intended scheduler: ");
        scanf("%d", &intended_sched);

        printf("Please set the process's priority (0~99): ");
        scanf("%d", &prio);

        // get previous scheduler
        int prev_sched_num = sched_getscheduler(pid);
        if(prev_sched_num == -1){
                printf("SYSCALL: sched_getscheduler Failed!\n");
                exit(1);
        }
        char prev_sched[20];
        getSched(prev_sched, prev_sched_num);

        // set new scheduler
        struct sched_param param;
    param.sched_priority = prio;
        if(sched_setscheduler(pid, intended_sched, &param) == -1){
                printf("SYSCALL: sched_setscheduler Failed!\n");
                exit(1);
        }

        // get current scheduler
    int curr_sched_num = sched_getscheduler(pid);
        if(curr_sched_num == -1){
                printf("SYSCALL: sched_getscheduler Failed!\n");
                exit(1);
        }
        char curr_sched[20];
        getSched(curr_sched, curr_sched_num);

        printf("\nSwitch from %s to %s\n", prev_sched, curr_sched);

    // print timeslice and priority
        struct timespec time_slice;
        if(sched_rr_get_interval(pid, &time_slice) == -1){
                printf("SYSCALL: sched_rr_get_interval Failed!\n");
                exit(1);
        }

        printf("Current scheduler's priority: %d\n", param.sched_priority);
        printf("Current scheduler's timeslice: %.2lf ms\n", time_slice.tv_sec * 1000.0 + time_slice.tv_nsec/1000000.0);

        printf("///////////////////////////////////////////////////////////\n");
        
        return 0;
}

