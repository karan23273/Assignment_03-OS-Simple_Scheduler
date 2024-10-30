We will have implemented both the processes in the a single file. This single file contains both the scheduler process and the shell process in a single file, both the processes are in a single file
Tokenise function - it is used to partition the string into the token so that it can run using the execvp command. For example the string "ls -l" will be written as ["ls", "-l"] after the tokenise function.
Roundrobin function - it is a scheduling algorithm which runs a given n number of processes for a given a time quantum, or runs until the process has completed execution.
For each process, the function first retrieves it from the runningQueue using front() and removes it with pop(). It tries to stop the process with kill(id, SIGSTOP), where id is the process ID. SIGSTOP is a signal to pause the process without terminating it.
The program expects two command-line arguments: NCPU (number of CPUs) and TSLICE (time slice for each process). These arguments are stored in n and tslice after conversion to integers.
Allocates memory for a command buffer (command) and initializes three queues:
readyQueue for processes waiting to run.
runningQueue for currently executing processes.
completeQueue for completed processes.
Sets STDIN_FILENO to non-blocking mode, allowing continuous checking for new commands without pausing the main loop.
submit Command: If the user inputs a command starting with "submit", it:
Extracts the command following "submit", forks a child process, and stops it immediately with SIGSTOP.
Initializes a Process structure with the process ID, arrival time, and other fields, then adds it to readyQueue
