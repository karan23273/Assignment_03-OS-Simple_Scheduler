#include<stdio.h>
#include"shared_data.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include<unistd.h>
#include<signal.h>
#include<stdlib.h>
#include<string.h>
#include<limits.h>
#include<sys/wait.h>
#include<sys/time.h>
#include<stdbool.h>
#include<sys/mman.h>
#include<sys/stat.h>
#include <fcntl.h>
#include<ctype.h> 
pid_t scheduler_pid;
#define MAX_CMD_LNTH 1000
#define HIS_LEN 1024
#define MAX_PROCESSES 10

Process p1[MAX_PROCESSES];
int total_process = 0;
pid_t pids[HIS_LEN][HIS_LEN];
long Execution_starts[HIS_LEN];
long Execution_time[HIS_LEN];

char history[HIS_LEN][MAX_CMD_LNTH] = {{'\0'}};
int command_index = 0;

void initialize_pid_array() {
    for (int i = 0; i < HIS_LEN; i++) {
        for (int j = 0; j < HIS_LEN; j++)
        {
            pids[i][j] = -1;
        
        }
        
    }
}

long get_time() {                  // calculating the time required for running the code
    struct timeval time_now;
    gettimeofday(&time_now, NULL);
    return time_now.tv_sec * 1000000L + time_now.tv_usec;
}

void add_process(pid_t id, char* cmd[], int priority, long burst_time) {
    if (total_process < MAX_PROCESSES) {
        p1[total_process].id = id;
        p1[total_process].priority = priority;
        p1[total_process].burst = burst_time;
        p1[total_process].remaining = burst_time;
        p1[total_process].completion = 0;
        p1[total_process].waiting = 0;

        for (int i = 0; cmd[i] != NULL && i < MAX_ARG; i++) {
            strncpy(p1[total_process].cmd[i], cmd[i], 256); // Copy command strings
            p1[total_process].cmd[i][255] = '\0'; // Ensure null termination
        }
        p1[total_process].cmd[MAX_ARG - 1][0] = '\0'; // Ensure NULL termination
        total_process++;
    }
}

void write_shared_memory() {
    int shm_file = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_file == -1) {
        perror("Shm open failed");
        return;
    }
    if (ftruncate(shm_file, SHM_SIZE) == -1) {
        perror("Shm ftruncate failed");
        return;
    }

    Process* share_data = (Process*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_file, 0);
    if (share_data == MAP_FAILED) {
        perror("mmap failure");
        return;
    }

    memcpy(share_data, p1, sizeof(Process) * total_process);
    
    munmap(share_data, SHM_SIZE);
    close(shm_file);
}


void new_line_remove(char* cmd){   // removing the spaces and the new line occurences from the arguement given to the shell 

    if (cmd == NULL || cmd[0] == '\0') {  // printing the error in case we have inputed nothing as the arguement
        printf("Arguments not found!\n");
        return;
    }

    
    while(*cmd == ' ' || *cmd == '\n') {  // removing the space and the new line comment
        cmd++; 
    }
    int length = strlen(cmd);

    while (length > 0 && (cmd[length - 1]==' ' || cmd[length-1]=='\n')) {

        cmd[length-1]='\0'; 
        length--;
    }
}


void addtoken(char* cmd, char** args, int *pipe_found, int *syz){ // this tokenizes the function into arguements that can be used to execute the program 
    char* token = strtok(cmd, " ");

    int i = 0;

    while (token!=NULL)
    {
        if (i >= MAX_ARG)
        {
            printf("Max arguments input reached! "); 
            break;
        
        }
        if (strcmp(token, "|") == 0)  // tokenizing based on size_t 
        {
            (*pipe_found) ++;
            args[i] =NULL;
            
        }else{
            args[i] = token;

        }
    
        token = strtok(NULL, " ");
        i++;
    }
    
    args[i] = NULL;
    (*syz) = i;

}


int command_execute2(char** args, int pipe_found, int n, int index){ // function defined for executing commands that implement a pipeline in it.

    long start_time;  // starting time for the code
    long end_time; 

    pid_t pids_pipe[HIS_LEN];

    int arg_index[pipe_found+1];
    arg_index[0] = 0;
    int j = 1;
    

    for (int i = 0; i < n; i++)
    {
        if (args[i] == NULL)
        {
            arg_index[j] = i+1;
            j++;
        }
        
    }
    
    int pip[2*pipe_found]; // creating a array of pipelines.
    for (int i = 0; i < pipe_found; i++) // intializing all the pipelines. 
    {
       if (pipe(pip + (i*2)) == -1) {
            perror("Pipe failure");
            exit(0);
        }else
        {
            pipe(pip + (i*2));
        }
    }
    
    start_time = get_time(); // starting the time we need to calculate.
    
    for (int i = 0; i <= pipe_found; i++)
    {
        pid_t pid = fork(); // creating the child process which we will be needing. 

        if (pid == 0)
        {
        
            if (i > 0) {

                dup2(pip[(i - 1) * 2], STDIN_FILENO);  // duplicating the file descriptors 
            }

            if (i < pipe_found) {
                dup2(pip[i * 2 + 1], STDOUT_FILENO); 
            }

                    
            for (int x = 0; x < 2*(pipe_found); x++) {  // closing the ends of the pipe we do not need 
                close(pip[x]); 
            }
            
            char **cmd1 = &args[arg_index[i]];
            if (execvp(cmd1[0], cmd1) == -1)  // executing the command and checking for errors.
            {
                printf("Command not found\n");
                exit(0);
            }else 
            {
                execvp(cmd1[0], cmd1);
            }
    

            
        }else if (pid < 0)
        {
            printf("Error 404! Process not executed\n");
            exit(0);
        }else
        {
            pids_pipe[i] = pid;   // storing the PIDS so that we print them later 
        }
                
    }
            
    for (int k = 0; k < 2 * pipe_found; k++) {     // closing the pipelines 
        close(pip[k]);
    }

    for (int o = 0; o <= pipe_found; o++) {
        waitpid(pids_pipe[o], NULL, 0); 
        // wait(NULL);
    }

    for (int i = 0; i <= pipe_found; i++)
    {
        pids[index][i] = pids_pipe[i];
    }
    
    end_time = get_time();

    Execution_starts[index] = start_time;
    Execution_time[index] = end_time - start_time;  // storing the execution time.
 

    return 1;
}

   
int command_execute(char** args, int n, int index){  // function defined for executing those commands who do not need a pipeline.

    long start_time;
    long end_time;

    int found_bg = 0; 
    int g = 0;
    while (args[g]!= NULL && g<n )
    {
        if (strcmp(args[g], "&") == 0)
        {
            args[g] = NULL;
            found_bg = 1;
            break;
        }
        g++;

    }


    pid_t pid = fork();
    
    if (pid == 0)
    {
        
        if (execvp(args[0], args) == -1){
            printf("Command not found\n");
            exit(0);
        }  
        

        // kill pids
        
        
    }else if (pid > 0)
    {
        // kill(pid, SIGSTOP);

        start_time = get_time();

        if (found_bg == 0)
        {
            waitpid(pid, NULL, 0);  // collecting the child process status. 
        }

        end_time = get_time();
        pids[index][0] = pid;
        Execution_starts[index] = start_time;
        Execution_time[index] = end_time - start_time;

    }else{
    
        printf("Error 404! Process not executed\n");
        exit(1);
    }

    return 1;

}


void push(char history[][MAX_CMD_LNTH], char* cmd){   // pushing all the commands in a array so that it can be used 
    int i = 0; 
    while (i<HIS_LEN && history[i][0] != '\0')
    {
        i++;
    }
    if (i >= HIS_LEN)
    {
        for (int j = 0; j < HIS_LEN -1; j++)
        {
            strncpy(history[j], history[j + 1], MAX_CMD_LNTH);
        }
        
        strcpy(history[HIS_LEN - 1], cmd);
    }else
    {
        strcpy(history[i], cmd);
    
    }
    
}

int is_integer(const char *str) {
    // Check for NULL or empty string
    if (str == NULL || *str == '\0') {
        return 0;
    }

    // Check for optional sign at the beginning
    if (*str == '-' || *str == '+') {
        str++;
    }

    // Ensure that there is at least one digit after an optional sign
    if (*str == '\0') {
        return 0;
    }

    // Check that the rest of the string is numeric
    while (*str) {
        if (!isdigit(*str)) {
            return 0;
        }
        str++;
    }
    return 1; // Returns 1 if valid integer
}

int get_priority(char **args) {
    int i = 0;

    while (args[i] != NULL) {
        i++;
    }

    if (i > 1 && is_integer(args[i - 1])) {
        return atoi(args[i - 1]); // Return priority if found
    }
    return 1;
}

char** get_command(char **args) {
    // Allocate memory for the command array
    char **command = (char **)malloc(MAX_ARG * sizeof(char *));
    if (command == NULL) {
        perror("Failed to allocate memory for command");
        return NULL;
    }

    int i = 1; // Start after "submit"
    int j = 0; // Index for command array

    // Iterate over args until NULL
    while (args[i] != NULL) {
        // If this is the last argument and it's an integer, skip it (priority)
        if (args[i + 1] == NULL && is_integer(args[i])) {
            break;
        }

        // Duplicate the argument into the command array
        command[j] = strdup(args[i]);
        if (command[j] == NULL) {
            perror("Failed to allocate memory for command argument");

            // Free any already allocated memory in case of error
            for (int k = 0; k < j; k++) {
                free(command[k]);
            }
            free(command);
            return NULL;
        }
        i++;
        j++;
    }
    command[j] = NULL; // Null-terminate the command array
    return command;
}

// Helper function to free allocated command memory
void free_command(char **command) {
    if (command == NULL) return;
    for (int i = 0; command[i] != NULL; i++) {
        free(command[i]);
    }
    free(command);
}





void show_pids(char history[HIS_LEN][MAX_CMD_LNTH], int n){
    printf("Exit Shell Successfuly!\n");
    for (int i = 0; i < n; i++)
    {
        printf("Command: %s", history[i]);
        printf("Process ID: ");
        for (int j = 0; j<HIS_LEN; j++)
        {
            if (pids[i][j] != -1)
            {
                printf("%d ", pids[i][j]);
                
            }else
            {
                break;
            }
            
            
        }printf("\n");
        
        printf("Execution starts: %ld\n", Execution_starts[i]);
        printf("Execution time: %ld\n", Execution_time[i]);
        printf("\n");
    }
    
}

void shell_loop(char* cmd, char** args){

    int status;
    initialize_pid_array();

    do {
        
        char pwd[PATH_MAX];
        int pipe_found = 0;
        int syz = 0;
        if (getcwd(pwd, sizeof(pwd)) != NULL) {

            printf("\nX@Shell%s:~ ", pwd);
            fflush(stdout);

        } else {

            perror("\nX@Shell/:~ ");
        
        }

         // consume the newline 
        if (fgets(cmd, MAX_CMD_LNTH, stdin) != NULL) {
            push(history, cmd);
            new_line_remove(cmd);
            addtoken(cmd, args, &pipe_found, &syz);
        
        } else {
            printf("EOF\n");
            exit(EOF);
        }


        if (strcmp(cmd, "exit")==0)
        {
            show_pids(history, command_index);
            break;
        }
        
        
        if(strcmp(args[0], "submit") == 0){
            
            char** a = get_command(args);
            add_process(1, a,get_priority(args) , 10);

            write_shared_memory();
            status = 1;
            
        }
        else if (strcmp(cmd,"history")==0)
        {
            for (int i = 0; history[i][0] != '\0'; i++) {
                printf("%d  %s", i+1, history[i]);
            }
            status = 1;

        }else
        {
            if (pipe_found != 0)
            {
                status = command_execute2(args,pipe_found, syz, command_index);

            }else
            {
        
                status = command_execute(args, syz, command_index);
            }
            
        }

        command_index++;
    } while (status);
    
}



void handler(int signal){
    switch (signal)
    {
    case SIGINT:
        // write(STDOUT_FILENO, " Caught signal interrupt\n",25);
        // show_pids(history, command_index);
        // exit(EXIT_SUCCESS);
        kill(scheduler_pid, SIGCONT);

    case SIGCHLD:
        write(STDOUT_FILENO, " Caught signal child\n",21);
        wait(NULL);
        // exit(0);
    default:
        break;
    }

}


int main(int argc, char *argv[]){


    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));  
    sa.sa_handler = handler;
    sigaction(SIGINT, &sa, NULL);  
    
    char *args[MAX_ARG];
    char cmd[MAX_CMD_LNTH];

    int no_of_cpu;
    long int time_slice;

    if (argc != 3)
    {
        // perror("Arguments not found");
        // exit(EXIT_SUCCESS);

        scanf("%d %ld", &no_of_cpu, &time_slice);
        getchar(); 
    }else
    {
        // perror("Arguments not found");

        no_of_cpu = atoi(argv[1]);
        time_slice = atoi(argv[2]); 

    }

    // char* cmd1[] = {"ls", "-l", NULL};
    // char* cmd2[] = {"echo", "Hello", "World", NULL};
    // char* cmd3[] = {"cat", "fib.c", NULL};

    // add_process(1, cmd1, 1, 10);
    // add_process(2, cmd2, 2, 20);
    // add_process(3, cmd3, 3, 30);
    // add_process(4, cmd3, 4, 40);
    // write_shared_memory();

    pid_t pid = fork();
    if(pid == 0){
        char cpus[100];
        char time[100];
        
        scheduler_pid = getpid();
        snprintf(cpus, sizeof(cpus), "%d", no_of_cpu);
        snprintf(time, sizeof(time), "%ld", time_slice);
        // char* argS[]= {"./Scheduler", "6", "100", NULL};
        char* argS[]= {"./r", cpus, time, NULL};
        kill(getpid(), SIGSTOP);
        if (execvp(argS[0], argS) == -1)
        {
            printf("Scheduler not found\n");
            exit(0);
        }
            
    }else if (pid>0)
    {
        // wait(NULL);
        shell_loop(cmd, args);
        
    }else
    {
        printf("Error 404! Process not executed\n");
        exit(1);   
    }


    return 0;
}
