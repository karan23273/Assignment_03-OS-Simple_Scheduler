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
        if (share_data[i].id != 0) {
            // printf("Process ID: %d\n", share_data[i].id);
            // printf("Priority: %d\n", share_data[i].priority);
            // printf("Burst Time: %ld\n", share_data[i].burst);
            // printf("Command: ");
            for (int j = 0; j < MAX_ARG; j++) {
                if (share_data[i].cmd[j][0] != '\0') {
                    printf("%s ", share_data[i].cmd[j]);
                } else {
                    break;
                }
            }
            // printf("\n");
            push(Ready_queue, &share_data[i]);
            // printf("Process %d pushed successfully\n", i + 1);
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

void execute(Queue* ready, Queue* complete, long timeQuantum) {
    Process* current;
    while (!empty(ready)) {
        current = front(ready);
        pop(ready);
        
        pid_t pid = fork();
        if (pid == 0) {
            // In child process
            char **args = convert_cmd_to_args(current->cmd);
            execvp(args[0], args);
            perror("exec failed");  // execvp only returns on error
            free_args(args);
            exit(EXIT_FAILURE);
        } else {
            // In parent process: Control the child execution
            current->id = pid;
            long startTime = get_time();
            int status;

            while (get_time() - startTime < timeQuantum && waitpid(pid, &status, WNOHANG) == 0) {
                usleep(1000); // Small delay to avoid busy waiting
            }

            if (waitpid(pid, &status, WNOHANG) == 0) {
                kill(pid, SIGSTOP); // Stop process if it exceeded timeQuantum
                current->remaining -= timeQuantum;
                if (current->remaining > 0) {
                    push(ready, current); // Requeue the process if not done
                }
            } else {
                // Process completed within time quantum
                current->completion = get_time();
                push(complete, current); // Move to completed queue
            }
        }
    }
}

void roundRobinScheduling(int* n, long timeQuantum) {
    int completed = 0;
    receive();  // Populate initial queue from shared memory
    execute(Ready_queue, Complete_queue, timeQuantum);

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
        fprintf(stderr, "Usage: %s <number_of_CPUs> <time_quantum>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int nCPUs = atoi(argv[1]);
    long timeQuantum = atol(argv[2]); 


    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    Ready_queue = createQueue();
    Running_queue = createQueue();
    Complete_queue = createQueue();

    roundRobinScheduling(&nCPUs, timeQuantum);

    cleanup_shared_memory();

    free(Ready_queue);
    free(Running_queue);
    free(Complete_queue);

    return 0;
}
