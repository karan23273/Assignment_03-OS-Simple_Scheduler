#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "shared_data.h"

#define QUEUE_SIZE 100

int shm_file;  // Shared memory file descriptor

// Function to get current time in microseconds
long get_time() {
    struct timeval time_now;
    gettimeofday(&time_now, NULL);
    return time_now.tv_sec * 1000000L + time_now.tv_usec;
}

// Queue structure
typedef struct Queue {
    Process arr[QUEUE_SIZE];
    int front;
    int top;
} Queue;

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

// Queue utility functions
bool overflow(Queue* q) {
    return (q->top == QUEUE_SIZE - 1);
}

bool empty(Queue* q) {
    return (q->front == -1 || q->front > q->top);
}

void push(Queue* q, Process* x) {
    if (overflow(q)) {
        printf("Queue Overflow\n");
        return;
    }
    if (q->front == -1) {
        q->front = 0;
    }
    q->top++;
    memcpy(&q->arr[q->top], x, sizeof(Process));
}

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

Process* front(Queue* q) {
    if (empty(q)) {
        printf("Queue is empty\n");
        return NULL;
    }
    return &q->arr[q->front];
}

// Global queues
Queue* Ready_queue;
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
        if (share_data[i].id != 0) {
            // Push the valid process to the ready queue
            push(Ready_queue, &share_data[i]);
        }
    }

    munmap(share_data, SHM_SIZE);
    close(shm_file);
}

// Convert process command to argument array
char** convert_cmd_to_args(char cmd[MAX_ARG][256]) {
    char **args = (char **)malloc((MAX_ARG + 1) * sizeof(char *));
    int i = 0;
    while (i < MAX_ARG && cmd[i][0] != '\0') {
        args[i] = strdup(cmd[i]);
        i++;
    }
    args[i] = NULL;
    return args;
}

void free_args(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }
    free(args);
}

void execute_round_robin(Queue* ready, Queue* complete, int n, long timeSlice) {
    while (!empty(ready)) {
        // Create a batch of up to n processes
        int batch_size = 0;
        pid_t pids[n];
        Process* current_batch[n];
        
        while (batch_size < n && !empty(ready)) {
            current_batch[batch_size] = front(ready);
            pop(ready);
            
            pid_t pid = fork();
            if (pid == 0) {
                // In child process
                char **args = convert_cmd_to_args(current_batch[batch_size]->cmd);
                execvp(args[0], args);
                perror("exec failed");
                exit(EXIT_FAILURE);
            } else {
                // Parent process
                current_batch[batch_size]->id = pid;
                pids[batch_size] = pid;
                batch_size++;
            }
        }
        
        // Run the batch for timeSlice
        long startTime = get_time();
        for (int i = 0; i < batch_size; i++) {
            kill(pids[i], SIGCONT); // Start each process in the batch
        }
        
        usleep(timeSlice * 1000);  // Run each batch for timeSlice ms
        
        // Check each process in the batch
        for (int i = 0; i < batch_size; i++) {
            int status;
            long runtime = get_time() - startTime;
            pid_t result = waitpid(pids[i], &status, WNOHANG);
            
            if (result == 0 && runtime >= timeSlice) {
                // TimeSlice expired, stop and requeue
                kill(pids[i], SIGSTOP);
                current_batch[i]->remaining -= timeSlice;
                
                if (current_batch[i]->remaining > 0) {
                    push(ready, current_batch[i]);  // Requeue if not finished
                } else {
                    current_batch[i]->completion = get_time();
                    push(complete, current_batch[i]);  // Move to Complete queue
                }
            } else if (result > 0) {
                // Process completed
                current_batch[i]->completion = get_time();
                push(complete, current_batch[i]);
            }
        }
    }
}

// Scheduler function
void roundRobinScheduling(int n, long timeSlice) {
    receive();  // Load processes into the Ready_queue from shared memory
    execute_round_robin(Ready_queue, Complete_queue, n, timeSlice);

    // Print completion times and calculate waiting times, etc.
    printf("\nProcess\tCompletion\n");
    while (!empty(Complete_queue)) {
        Process* current = front(Complete_queue);
        printf("P%d\t%ld\n", current->id, current->completion);
        pop(Complete_queue);
    }
}

void cleanup_shared_memory() {
    if (shm_unlink(SHM_NAME) == -1) {
        perror("Failed to remove shared memory");
    }
}

void signal_handler(int signum) {
    printf("Received signal %d, cleaning up shared memory.\n", signum);
    cleanup_shared_memory();
    exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <numCPUs> <timeQuantum>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int nCPUs = atoi(argv[1]);
    long timeQuantum = atol(argv[2]); 

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    Ready_queue = createQueue();
    Complete_queue = createQueue();

    roundRobinScheduling(nCPUs, timeQuantum);

    cleanup_shared_memory();

    free(Ready_queue);
    free(Complete_queue);

    return 0;
}
