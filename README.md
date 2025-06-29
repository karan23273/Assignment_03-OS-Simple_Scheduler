We will have implemented both the processes in the a single file. This single file contains both the scheduler process and the shell process in a single file, both the processes are in a single file <br>
**Tokenise function** - it is used to partition the string into the token so that it can run using the execvp command. For example the string "ls -l" will be written as ["ls", "-l"] after the tokenise function. <br>
**Roundrobin function**- it is a scheduling algorithm which runs a given n number of processes for a given a time quantum, or runs until the process has completed execution.<br>
For each process, the function first retrieves it from the runningQueue using front() and removes it with pop(). It tries to stop the process with kill(id, SIGSTOP), where id is the process ID. SIGSTOP is a signal to pause the process without terminating it.
The program expects two command-line arguments: NCPU (number of CPUs) and TSLICE (time slice for each process). These arguments are stored in n and tslice after conversion to integers.<br>
Allocates memory for a command buffer (command) and initializes three queues <br>
**readyQueue** for processes waiting to run <br> **runningQueue** for currently executing processes<br> **completeQueue** for completed processes<br>
Sets STDIN_FILENO to non-blocking mode, allowing continuous checking for new commands without pausing the main loop.<br>
**submit Command**: If the user inputs a command starting with "submit" it Extracts the command following "submit", forks a child process, and stops it immediately with SIGSTOP.<br>
Initializes a Process structure with the process ID, arrival time, and other fields, then adds it to readyQueue. If readyQueue is non-empty and no process is currently executing, it Calls round_robin() to start executing the next process. Records the start time for this execution (temp) and sets executing to 1.<br>
If the time slice (tslice) has elapsed since starting the current process, it calls stop() to stop the current process and sets executing back to 0 to pick the next one.<br>
