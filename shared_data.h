// shared_data.h

#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>

#define MAX_PROCESSES 10
#define MAX_ARG 10
#define SHM_NAME "/my_shared_memory"
#define SHM_SIZE sizeof(Process) * MAX_PROCESSES

typedef struct {
    pid_t id;                   // Process ID
    int priority;               // Process priority
    char cmd[MAX_ARG][256];     // Process command with arguments
    long int burst;             // Burst time
    long int remaining;         // Remaining burst time
    long int completion;        // Completion time
    long int waiting;           // Waiting time
} Process;

#endif // SHARED_DATA_H
