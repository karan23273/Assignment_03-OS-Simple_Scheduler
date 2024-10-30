#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef struct
{
    pid_t id;
    char command[256];
    long int arrival;
    long int completion;
    long int execution;
    long int waiting;
} Process;

char *command;
int n;
int tslice;

// Queue structure
typedef struct
{
    Process arr[100];
    int front;
    int rear;
    int size;
} Queue;

// Function to create a new Queue
void createQueue(Queue *q)
{
    q->front = 0;
    q->rear = 0;
    q->size = 0;
}

// Queue utility functions
bool overflow(Queue *q)
{
    return (q->size == 100);
}

bool empty(Queue *q)
{
    return (q->front == q->rear);
}

void push(Queue *q, Process x)
{
    if (overflow(q))
    {
        printf("Queue Overflow\n");
        return;
    }

    q->arr[q->rear] = x;
    q->rear++;
    q->size++;
}

void pop(Queue *q)
{
    if (empty(q))
    {
        printf("Queue is empty\n");
        return;
    }

    q->front++;
    q->size--;
}

void clearQueue(Queue *q)
{
    q->front = 0;
    q->rear = 0;
    q->size = 0;
}

Process front(Queue *q)
{
    Process p = {0};
    if (empty(q))
    {
        return p;
    }

    return q->arr[q->front];
}

Queue readyQueue;
Queue runningQueue;
Queue completeQueue;

long get_time()
{
    struct timeval time_now;
    gettimeofday(&time_now, NULL);
    return time_now.tv_sec * 1000000L + time_now.tv_usec;
}

char **tokenise(char *str)
{
    char *str2 = strdup(str);

    char **arr = NULL;

    char *ele = strtok(str2, " ");

    int i = 0;

    while (ele != NULL)
    {
        arr = realloc(arr, sizeof(char *) * (i + 1));
        arr[i] = strdup(ele);
        i++;

        ele = strtok(NULL, " ");
    }

    char **temp = realloc(arr, sizeof(char *) * (i + 1));

    arr = temp;
    arr[i] = NULL;

    return arr;
}

void round_robin()
{
    while (runningQueue.size != n && !empty(&readyQueue))
    {
        Process p = front(&readyQueue);
        pop(&readyQueue);

        kill(p.id, SIGCONT);

        p.execution += tslice;

        push(&runningQueue, p);
    }

    for (int i = readyQueue.front; i < readyQueue.rear; i++)
    {
        readyQueue.arr[i].waiting += tslice;
    }
}

void stop()
{
    for (int i = runningQueue.front; i < runningQueue.rear; i++)
    {
        Process p = front(&runningQueue);
        pop(&runningQueue);

        int id = p.id;

        if (kill(id, SIGSTOP) == -1)
        {
            if (errno == ESRCH)
            {
                p.completion = get_time();
                push(&completeQueue, p);
            }
            else
            {
                printf("Failed to stop process %d.\n", id);
                exit(0);
            }
        }
        else
        {
            push(&readyQueue, p);
        }
    }

    clearQueue(&runningQueue);
}

void sigchld()
{
    int status;

    while (waitpid(-1, &status, WNOHANG) > 0)
    {
    }
}

void sigint()
{

    printf("\n\n");
    for (int i = completeQueue.front; i < completeQueue.rear; i++)
    {
        printf("Command: %s \n", completeQueue.arr[i].command);
        printf("Process ID: %d \n", completeQueue.arr[i].id);

        printf("Starting time: %ld \n", completeQueue.arr[i].arrival);
        printf("Completion time: %ld\n", completeQueue.arr[i].completion);
        printf("Execution time: %ld\n", completeQueue.arr[i].execution);
        printf("Waiting time: %ld\n", completeQueue.arr[i].waiting);

        printf("\n");
    }

    printf("Exit Shell Successfuly!\n");

    exit(0);
}

void signal_handler()
{

    struct sigaction sig1;
    memset(&sig1, 0, sizeof(sig1));
    sig1.sa_handler = sigint;

    if (sigaction(SIGINT, &sig1, NULL) != 0)
    {
        printf("Failed to handle signal.\n");
        exit(0);
    }

    struct sigaction sig2;
    sig2.sa_handler = sigchld;
    sigemptyset(&sig2.sa_mask);
    sig2.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sig2, NULL) == -1)
    {
        printf("Failed to handle signal.\n");
        exit(0);
    }
}

int main(const int argc, char const *argv[])
{
    // Check arguments
    if (argc != 3)
    {
        printf("NCPU and TSLICE are required.\n");
        exit(0);
    }

    // Converting to integers
    n = atoi(argv[1]);
    tslice = atoi(argv[2]);

    signal_handler();

    command = (char *)malloc(100 * sizeof(char));
    if (!command)
    {
        perror("Failed to allocate memory for command");
        exit(1);
    }

    createQueue(&readyQueue);
    createQueue(&runningQueue);
    createQueue(&completeQueue);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    int executing = 0;
    long temp;

    printf("Shell~ ");

    while (1)
    {

        if (fgets(command, 100, stdin) != NULL)
        {
            command[strcspn(command, "\n")] = '\0';

            if (strncmp(command, "submit", 6) == 0)
            {
                char **args = tokenise(command + 7);

                printf("Received submit command: %s\n", args[0]);

                int id = fork();
                if (id < 0)
                {
                    printf("Fork failed.\n");
                }
                else if (id == 0)
                {
                    execvp(args[0], args);
                    printf("Exec Failed.\n");
                    exit(0);
                }

                kill(id, SIGSTOP);

                Process p;
                p.id = id;
                strcpy(p.command, command);
                p.arrival = get_time();
                p.waiting = 0;
                p.completion = 0;
                p.execution = 0;

                push(&readyQueue, p);
            }
            else
            {
                printf("Unrecognized command: %s\n", command);
            }
            printf("Shell~ ");
        }
        else
        {
        }

        if (!empty(&readyQueue) && executing == 0)
        {
            round_robin();
            temp = get_time();
            executing = 1;
        }

        if (executing == 1 && (get_time() - temp) / 1000 >= tslice)
        {
            stop();
            executing = 0;
        }
    }

    free(command);
    return 0;
}