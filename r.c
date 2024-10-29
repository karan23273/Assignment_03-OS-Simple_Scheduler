#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include<sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "shared_data.h"

#define QUEUE_SIZE 100

// Global variable to hold the shared memory file descriptor
int shm_file;

// Process structure for shared memory
long get_time() {                  // calculating the time required for running the code
    struct timeval time_now;
    gettimeofday(&time_now, NULL);
    return time_now.tv_sec * 1000000L + time_now.tv_usec;
}

// Queue structure
typedef struct Queue {
    Process arr[QUEUE_SIZE];  // Change to store Process objects directly
    int front;
    int top;
} Queue;

// Function prototypes
Queue* createQueue();
bool overflow(Queue* q);
bool empty(Queue* q);
void push(Queue* q, Process* x);
void pop(Queue* q);
Process* front(Queue* q);
void read_shared_memory();
void cleanup_shared_memory();
void signal_handler(int signum);

// Function to create a new Queue
Queue* createQueue() {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    if (!q) {
        perror("Failed to create queue");
        exit(EXIT_FAILURE);
    }
    q->front = -1;
    q->top = -1;
    return q;
}

// Function to check for queue overflow
bool overflow(Queue* q) {
    return (q->top == QUEUE_SIZE - 1);
}

// Function to check if queue is empty
bool empty(Queue* q) {
    return (q->front == -1 || q->front > q->top);
}

// Function to push a process into the queue
void push(Queue* q, Process* x) {
    if (overflow(q)) {
        printf("Queue Overflow\n");
        return;
    }
    if (q->front == -1) {
        q->front = 0;
    }
    q->top++;
    // Store a copy of the Process structure in the queue
    memcpy(&q->arr[q->top], x, sizeof(Process));
}

// Function to pop a process from the queue
void pop(Queue* q) {
    if (empty(q)) {
        printf("Queue is empty\n");
        return;
    }
    q->front++;
    if (q->front > q->top) {
        q->front = -1;
        q->top = -1;
    }
}

// Function to get the front process from the queue
Process* front(Queue* q) {
    if (empty(q)) {
        printf("Queue is empty\n");
        return NULL;
    }
    return &q->arr[q->front];
}

Queue* Ready_queue;
Queue* Running_queue;
Queue* Complete_queue;

// Function to read process data from shared memory
void receive() {
    shm_file = shm_open(SHM_NAME, O_RDONLY, 0666);
    if (shm_file == -1) {
        perror("Shm open failed");
        return;
    }

    Process* share_data = (Process*)mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, shm_file, 0);
    if (share_data == MAP_FAILED) {
        perror("mmap failure");
        close(shm_file);
        return;
    }

    printf("Reading data from shared memory:\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (share_data[i].id != 0) { // Check if the process is initialized
            printf("Process ID: %d\n", share_data[i].id);
            printf("Priority: %d\n", share_data[i].priority);
            printf("Burst Time: %ld\n", share_data[i].burst);
            printf("Command: ");
            for (int j = 0; j < MAX_ARG; j++) {
                if (share_data[i].cmd[j][0] != '\0') { // Check if the command is valid
                    printf("%s ", share_data[i].cmd[j]);
                } else {
                    break; // Break on the first empty command
                }
            }
            printf("\n");
            push(Ready_queue, &share_data[i]); // Store a copy of the process
            printf("Process %d pushed successfully\n", i + 1);
        }
    }

    munmap(share_data, SHM_SIZE);
    close(shm_file);
}


char** convert_cmd_to_args(char cmd[MAX_ARG][256]) {
    char **args = (char **)malloc((MAX_ARG + 1) * sizeof(char *)); // +1 for NULL termination
    if (args == NULL) {
        perror("Failed to allocate memory for args");
        return NULL;
    }

    int i = 0;
    while (i < MAX_ARG && cmd[i][0] != '\0') { // Check for non-empty strings
        args[i] = (char *)malloc(strlen(cmd[i]) + 1); // +1 for NULL terminator
        if (args[i] == NULL) {
            perror("Failed to allocate memory for args[i]");
            // Free previously allocated memory before returning
            for (int j = 0; j < i; j++) {
                free(args[j]);
            }
            free(args);
            return NULL;
        }
        strcpy(args[i], cmd[i]); // Copy the string
        i++;
    }

    args[i] = NULL; // Null-terminate the args array
    return args;
}

void free_args(char **args) {
    if (args != NULL) {
        for (int i = 0; args[i] != NULL; i++) {
            free(args[i]); // Free each string
        }
        free(args);
    }
}


void execute(int *completed, int n, long timeQuantum){
    int i = 0;
    long currentTime;
    while (i<n && !empty(Ready_queue))
    {
        Process* p1 = front(Ready_queue);
        push(Running_queue, p1);
        pop(Ready_queue);
        i++;

        

        if (p1->remaining > 0) {
            // Execute the process for a time quantum or its remaining burst time

            if (p1->id != 0)
            {
                currentTime = get_time();

                char **args= convert_cmd_to_args(p1->cmd);
                if (execvp(args[0],args) == -1)
                {
                    perror("Process not executed");
                }
                p1->id = getpid();
                free_args(args);
                
            }else
            {
                kill(p1->id, SIGCONT);
            }
            
            
            
            int executionTime = (p1->remaining < timeQuantum) ? p1->remaining : timeQuantum;
            p1->remaining -= executionTime;
            currentTime += executionTime;


            // If the process is completed
            if (p1->remaining == 0) {
                p1->completion = currentTime;
                (*completed)++;
                push(Complete_queue, p1);

            }else
            {
                push(Ready_queue,p1);

                kill(p1->id, SIGSTOP);
            }
            
        }else
        {
            push(Complete_queue, p1);
        }
        

    }
    
}


void roundRobinScheduling( int* n, long timeQuantum) {
    long currentTime;
    int completed = 0;

    // Loop until all processes are completed
    while (completed < *n) {
        
        // receive
        receive();

        execute(&completed ,*n, timeQuantum);


        // for (int i = 0; i < n; i++) {
        //     // Check if the process has remaining burst time
        //     if (processes[i].remaining > 0) {
        //         // Execute the process for a time quantum or its remaining burst time
        //         int executionTime = (processes[i].remaining < timeQuantum) ? processes[i].remaining : timeQuantum;
        //         processes[i].remaining -= executionTime;
        //         currentTime += executionTime;

        //         // Print the process execution for demonstration
        //         printf("Time %d - %d: P%d\n", currentTime - executionTime, currentTime, processes[i].id);

        //         // If the process is completed
        //         if (processes[i].remaining == 0) {
        //             processes[i].completion = currentTime;
        //             completed++;
        //         }
        //     }
        // }
    }

    // Calculate waiting time for each process
    // for (int i = 0; i < *n; i++) {
    //     processes[i].waiting = processes[i].completion - processes[i].burst;
    // }

    // // Display process details
    // printf("\nProcess\tBurst\tCompletion\tWaiting\n");
    // for (int i = 0; i < *n; i++) {
    //     printf("P%d\t%ld\t%ld\t\t%ld\n", processes[i].id, processes[i].burst, processes[i].completion, processes[i].waiting);
    // }

    // // Calculate and display average waiting and completion times
    // float totalWaitingTime = 0;
    // float totalCompletionTime = 0;
    // for (int i = 0; i < *n; i++) {
    //     totalWaitingTime += processes[i].waiting;
    //     totalCompletionTime += processes[i].completion;
    // }
    // printf("\nAverage Waiting Time: %.2f\n", totalWaitingTime / *n);
    // printf("Average Completion Time: %.2f\n", totalCompletionTime / *n);
}





void cleanup_shared_memory() {
    // Remove shared memory
    if (shm_unlink(SHM_NAME) == -1) {
        perror("Failed to remove shared memory");
    }
}

// Signal handler for cleaning up
void signal_handler(int signum) {
    printf("Received signal %d, cleaning up shared memory.\n", signum);
    cleanup_shared_memory();
    exit(EXIT_SUCCESS);
}

// Main function
int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <number_of_CPUs> <time_quantum>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int nCPUs = atoi(argv[1]);
    long timeQuantum = atol(argv[2]); 
    if (nCPUs <= 0 || timeQuantum <= 0) {
        fprintf(stderr, "Invalid arguments: number of CPUs and time quantum must be positive integers.\n");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    Ready_queue = createQueue();
    Running_queue = createQueue();
    Complete_queue = createQueue();

    roundRobinScheduling(&nCPUs, timeQuantum);

    // receive();



    if (empty(Ready_queue)){
        printf("Submission not successful\n");
    } else {
        printf("Submission successful\n");
        while (!empty(Ready_queue)) {
            Process* current_process = front(Ready_queue);
            printf("Command: %s\n", current_process->cmd[0]);
            pop(Ready_queue);
        }
    }

    free(Ready_queue);
    free(Running_queue);
    free(Complete_queue);

    cleanup_shared_memory();

    return 0;
}
