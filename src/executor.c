#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/wait.h> //added for wait() call
#include <sys/types.h> //for pid
#include <string.h>

#include "executor.h"
/*
double check if we need the background process implementation in spawn command (dont think so)
*/

/*
Notes: Work on logic to assign status of job (running, done, etc.)
Reset jobs_count after a job has ended
What happends to job_id attriubute after the job has ended?
See if I can add args to the jobs output
*/

/*
use > or < for i/o redirection the same as &
if < is detected then activate logic
*/
static job_t *jobs = NULL;
static size_t jobs_count = 0; // good to use size_t since we don't know how many jobs there will be
static int next_job_id = 1;

/**
 * @brief Executes a single, simple command in another process
 * @param cmd
 * @param args
 * @param in
 * @param out
 * @param bg
 * @return
 */
int execute_command(char* cmd, char** args, int in, int out, int bg) { 
    //fork a child process
    pid_t pid = fork();
    if (pid < 0) {
        //error occured
        fprintf(stderr, "Fork Failed");
        return 1;
    } else if (pid == 0){
        //child process
        //handle files first
        if (in != STDIN_FILENO) {
            dup2(in, STDIN_FILENO);
            close(in);
        }
        if (out != STDOUT_FILENO) {
            dup2(out, STDOUT_FILENO);
            close(out);
        }
        execvp(cmd, args);
        fprintf(stderr, "execvp failed");
        exit(EXIT_FAILURE);
    } else {
        if (!bg){
            //if it is a foreground process (no &)
            wait (NULL);
            printf("Child Complete \n");
        } else {
            //if it is a background porcess (with the &)
            printf("Background process is running \n");
            //only add jobs for bg processes
            add_job(pid, cmd, args);   // add_job should strdup the string
            printf("[%d] %d\n", (int)pid, (int)pid);
        }

    }

    return EXIT_SUCCESS;


}

/**
 * @brief Executes a command pipeline.
 * @param l A pointer to a cmdline structure containing the parsed command.
 * @return 0 on success, or a non-zero value on error.
 */
int execute(struct cmdline *l){

    printf("TODO: Execute the command\n");
    int i,j;
    if (l->in) printf("in: %s\n", l->in);
    if (l->out) printf("out: %s\n", l->out);
    if (l->bg) printf("background (&)\n");

    /* Display each command of the pipe */
    for (i=0; l->seq[i]!=0; i++) {
        char **cmd = l->seq[i];
        printf("seq[%d]: ", i);
        for (j=0; cmd[j]!=0; j++) {
            printf("'%s' ", cmd[j]);
        }
        printf("\n");
    }
    // return EXIT_SUCCESS;

    /*currently, with skeleton code, echo coen346 returns 
    TODO: Execute the command
    seq[0]: 'echo' 'coen346' 
    so handle this case */

    if (l-> seq[0] == NULL){
        return EXIT_SUCCESS;
    }

    //execute jobs command if it is called
    char **first = l->seq[0];
    if (first && first[0] && strcmp(first[0], "jobs") == 0) {
        print_jobs();
        return EXIT_SUCCESS;
    }

    //if pipe "|" detected for two commands ONLY, call execute_pipe func 
    //after adding multpipe function, use count_commands
    //call mult_pipe if multiple pipes/commands

    int pipe_num = count_commands(l);
    if (pipe_num <= 0) return EXIT_SUCCESS;
    if (pipe_num == 2) return execute_pipe(l);
    if (pipe_num > 2)  return mult_pipes(l);
    //if pipe_num is 1 just continue below as normal

    //get first command
    char **cmd = l->seq[0];
    // return execute_command(cmd[0], cmd, STDIN_FILENO, STDOUT_FILENO, l->bg);

    int output_fd = wr_to_file(l);  // to open the file (then write)
    int input_fd = rd_from_file(l); // to open the input file

    int result = execute_command(cmd[0], cmd, input_fd, output_fd, l->bg );

    //close the files after operations
    if (output_fd != STDOUT_FILENO) close(output_fd);
    if (input_fd != STDIN_FILENO) close(input_fd);

    return result;

}

//duplicate the command string and add to job
//use realloc and free to add and remove from storage
// --------------------------JOBS LOGIC-------------------------------

void add_job(pid_t pid, char* cmd, char** args){
    //allocate the memory to add jobs
    //realloc will resize the block to append new jobs
    //function is void *realloc( void *ptr, size_t new_size );
    // so to get size of a new job, do number of jobs * the size of one job
    job_t *tmp = realloc(jobs, (jobs_count+1) *sizeof(job_t) );
    
    if (!tmp) {
        fprintf(stderr, "realloc");
        return;
    }

    jobs = tmp;

    //Fill in the new jobs
    jobs[jobs_count].job_id = next_job_id++;
    jobs[jobs_count].pid = pid;
    jobs[jobs_count].state = Running;
    //check whether cmd is null. If not null then duplicate the cmd string
    // jobs[jobs_count].cmdstr = cmd ? strdup(cmd) : NULL; 
    //build full string command argument
    //get total length and allocate memory 
    //first check null 
    if (args && args[0]){
        size_t total_len = 0;
        for (int i = 0; args[i]; i++){
            total_len += strlen(args[i]) + 1;
        }

    //copy the full command with strcpy since we allocate memory
    char* full_cmd = malloc(total_len);
    if (full_cmd){
        //strcpy(destination, source)
        strcpy(full_cmd, args[0]);
        for (int i = 1; args[i] != NULL; i++){
            strcat(full_cmd, " ");
            strcat(full_cmd,  args[i]);
        }
        jobs[jobs_count].cmdstr = full_cmd; //copies args as well
    } else {
        jobs[jobs_count].cmdstr = strdup(cmd);
    }
    } else {
        jobs[jobs_count].cmdstr = NULL;
    }
    
    if (cmd && !jobs[jobs_count].cmdstr) {
        fprintf(stderr, "add jobs failed");
        jobs[jobs_count].cmdstr = strdup("");
    }
    jobs_count++;
}

//clean up jobs not running from the jobs struct
void cleanup_finished_jobs() {
    if (jobs_count == 0) return;

    //use rj to represent a running job index
    //i represents all jobs
    size_t rj = 0;
    for (size_t i = 0; i < jobs_count; ++i) {
        int status = 0;
        //waitpid func info obtained from IBM.com
        // 0 means the child process is still running
        //pid means that the process has terminated
        pid_t r = waitpid(jobs[i].pid, &status, WNOHANG);
        if (r == 0) {
            //keep the job if it still running
            if (rj != i) jobs[rj] = jobs[i];
            jobs[i].state = Running;
            rj++;
        } else if (r == jobs[i].pid) {
            //free storage if job is finished 
            free(jobs[i].cmdstr);
            jobs[i].state = Done;
            
        } else {
            //keep the job if there is an error (not equal to 0 or pid)
            if (rj != i) jobs[rj] = jobs[i];
            jobs[i].state = Stopped;
            rj++;
        }
    }

    if (rj == 0) {
        free(jobs);
        jobs = NULL;
        jobs_count = 0;
        next_job_id = 1; // After testing prev version, reset job id for accurate indexing
    } else if (rj != jobs_count) {
        //resize to the current number of jobs
        job_t *tmp = realloc(jobs, rj * sizeof(job_t));
        if (tmp) jobs = tmp;
        jobs_count = rj;
    }
}

void print_jobs(void) {
    cleanup_finished_jobs();
    for (size_t i = 0; i < jobs_count; ++i) {
        const char* print_state;
        switch (jobs[i].state){
            case Running:
                print_state = "Running";
                break;
            case Stopped:
                print_state = "Stopped";
                break;
            case Done:
                print_state = "Done";
                break;
            default:
                print_state = "Unknown";
        }
        if (jobs[i].state == Running) {
            printf("[%d] %d %s %s\n", jobs[i].job_id, (int)jobs[i].pid,
                   jobs[i].cmdstr ? jobs[i].cmdstr : "", print_state);
        }
    }
}

// --------------------------FILE REDIRECTION LOGIC-------------------------------
//l->in contains file name for input redirection
//l-> out for output redirection (write file)

//write to file should return the file descriptor fd, and should now take input and write to the file
int wr_to_file(struct cmdline *l){
    if(l->out){
        int fd = open(l->out, O_WRONLY | O_CREAT | O_TRUNC, 0644); //taken from man7.org
        //write only to file, or create the file, clear it then write
        if (fd == -1){
            fprintf(stderr, "Failed to open the file");
            return -1;
        } else {
            return fd;
        }
    }
    return STDOUT_FILENO;
}

int rd_from_file(struct cmdline *l){
    if(l->in){
        int fd = open(l->in, O_RDONLY, 0644); //taken from man7.org
        //read only from file
        if (fd == -1){
            fprintf(stderr, "Failed to open the file");
            return -1;
        } else {
            return fd;
        }
    }
    return STDIN_FILENO;
}

//---------------------------PIPE (FOR 2 CMDS)---------------------------
//notes: one end of the pipe writes and the other reads from the pipe (connecting two cmds)
//write -> pipefd[0]
//read -> pipefd[1]

/*
how it works:
instead of writing to the terminal, write output to a file
read from file and use for next command
so we need two process creations in this func
bypass calling execute_command()
*/

int execute_pipe(struct cmdline *l) {
    if (!l || !l->seq || !l->seq[0] || !l->seq[1]) return EXIT_FAILURE;

    //get cmds from seq array
    char **cmd1 = l->seq[0];
    char **cmd2 = l->seq[1];
    if (cmd1 == NULL || cmd2 == NULL) return EXIT_FAILURE;

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        fprintf(stderr, "pipe");
        return EXIT_FAILURE;
    }

    // Open redirection files: input applies to first command, output to second 
    int input_fd = rd_from_file(l);
    int output_fd = wr_to_file(l);
    if (input_fd == -1 || output_fd == -1) {
        if (input_fd != STDIN_FILENO && input_fd != -1) close(input_fd);
        if (output_fd != STDOUT_FILENO && output_fd != -1) close(output_fd);
        close(pipefd[0]); close(pipefd[1]);
        return EXIT_FAILURE;
    }

    //only add functionality to child processes
    //if parent process or otherwise, close files
    pid_t p1 = fork();
    if (p1 < 0) {
        fprintf(stderr, "pipe");
        close(pipefd[0]); 
        close(pipefd[1]);
        if (input_fd != STDIN_FILENO) close(input_fd);
        if (output_fd != STDOUT_FILENO) close(output_fd);
        return EXIT_FAILURE;
    }

    //output from cmd1 (so write for pipe)
    if (p1 == 0) {
        // Child 1: stdin <- input_fd, stdout -> pipe write end 
        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        dup2(pipefd[1], STDOUT_FILENO);
        //close unused write and close original read
        close(pipefd[0]); 
        close(pipefd[1]);
        if (output_fd != STDOUT_FILENO) close(output_fd);
        execvp(cmd1[0], cmd1);
        fprintf(stderr, "execvp failed\n");
        exit(EXIT_FAILURE);
    }

    //cleanup for parent, same as above
    pid_t p2 = fork();
    if (p2 < 0) {
        fprintf(stderr, "pipe");
        close(pipefd[0]); 
        close(pipefd[1]);
        if (input_fd != STDIN_FILENO) close(input_fd);
        if (output_fd != STDOUT_FILENO) close(output_fd);
        return EXIT_FAILURE;
    }

    //input for cmd2 (so read from pipe)
    if (p2 == 0) {
        //Child 2: stdin <- pipe read end, stdout -> output_fd 
        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }
        dup2(pipefd[0], STDIN_FILENO);
        //close unused write and close original read file
        close(pipefd[0]); 
        close(pipefd[1]);
        if (input_fd != STDIN_FILENO) close(input_fd);
        execvp(cmd2[0], cmd2);
        fprintf(stderr, "execvp failed\n");
        exit(EXIT_FAILURE);
    }

    //Parent: close fds used for piping and redirection 
    close(pipefd[0]); 
    close(pipefd[1]);
    if (input_fd != STDIN_FILENO) close(input_fd);
    if (output_fd != STDOUT_FILENO) close(output_fd);

    if (!l->bg) {
        waitpid(p1, NULL, 0);
        waitpid(p2, NULL, 0);
    } else {
        //background: add first child as a job
        printf("Background process is running \n");
        add_job(p1, cmd1[0], cmd1);
        printf("[%d] %d\n", (int)p1, (int)p1);
    }

    return EXIT_SUCCESS;
}

//-------------------------MULTIPLE-PIPE-----------------------
/*
note to self: to create multiple pipes, you need to know the amount of commands
use a loop to determine how many commands or how many pipes
set up pipeline with the loop 
create an array stucture of all pipe pairs (fds)
create a command spawn_command to execute a single command with specified in/out fds
ensure the child closes all pipe fds that it does not need
*/

//count number of commands
//get size of sequence
int count_commands(struct cmdline *l) {
    //return 0 if cmdline is empty 
    if (!l || !l->seq) return 0;
    int n = 0;
    while (l->seq[n]) ++n;
    return n;
}
//if n=2 call execute_pipe()

pid_t spawn_command(char **cmd, int in_fd, int out_fd, int bg, int *all_pipes, int all_pipes_len) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        /* Child: set up stdin/stdout */
        if (in_fd != STDIN_FILENO) {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }
        if (out_fd != STDOUT_FILENO) {
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }

        /* Close all pipe fds inherited from parent */
        if (all_pipes) {
            for (int i = 0; i < all_pipes_len; ++i) {
                if (all_pipes[i] >= 0) close(all_pipes[i]);
            }
        }

        execvp(cmd[0], cmd);
        fprintf(stderr, "execvp failed\n");
        exit(EXIT_FAILURE);
    }
    return pid;
}


int mult_pipes(struct cmdline *l){
    int n = count_commands(l);
    if (n <= 0) return EXIT_SUCCESS;
    if (n == 1) {
        /* fallback to single command */
        char **cmd = l->seq[0];
        int out_fd = wr_to_file(l);
        int in_fd = rd_from_file(l);
        if (in_fd == -1 || out_fd == -1) return EXIT_FAILURE;
        int r = execute_command(cmd[0], cmd, in_fd, out_fd, l->bg);
        if (out_fd != STDOUT_FILENO) close(out_fd);
        if (in_fd != STDIN_FILENO) close(in_fd);
        return r;
    }

    /* create pipes: (n-1) pipes => (n-1)*2 fds */
    int pipes_count = n - 1;
    int *pipes = malloc(sizeof(int) * pipes_count * 2);
    if (!pipes) return EXIT_FAILURE;
    for (int i = 0; i < pipes_count; ++i) {
        if (pipe(&pipes[i*2]) == -1) {
            perror("pipe");
            /* cleanup previously created pipes */
            for (int j = 0; j < i; ++j) { close(pipes[j*2]); close(pipes[j*2+1]); }
            free(pipes);
            return EXIT_FAILURE;
        }
    }

    /* compute child pids */
    //make space for children
    pid_t *pids = malloc(sizeof(pid_t) * n);
    if (!pids) { free(pipes); return EXIT_FAILURE; }

    for (int i = 0; i < n; ++i) {
        int in_fd, out_fd;
        if (i == 0) {
            in_fd = rd_from_file(l);
        } else {
            in_fd = pipes[(i-1)*2]; /* read end of previous pipe */
        }
        if (i == n-1) {
            out_fd = wr_to_file(l);
        } else {
            out_fd = pipes[i*2 + 1]; /* write end of current pipe */
        }

        if (in_fd == -1 || out_fd == -1) {
            /* cleanup: close opened fds and pipes */
            for (int k = 0; k < pipes_count; ++k) { close(pipes[k*2]); close(pipes[k*2+1]); }
            free(pipes);
            free(pids);
            return EXIT_FAILURE;
        }

        /* Spawn the command; pass all pipe fds so child can close unused ones */
        pids[i] = spawn_command(l->seq[i], in_fd, out_fd, l->bg, pipes, pipes_count*2);
        if (pids[i] < 0) {
            perror("fork");
            /* cleanup */
            for (int k = 0; k < pipes_count; ++k) { close(pipes[k*2]); close(pipes[k*2+1]); }
            free(pipes); 
            free(pids);
            return EXIT_FAILURE;
        }

        /* Parent closes fds it no longer needs: after spawning child i,
           the parent can close the read end of previous pipe and the write
           end of current pipe if they won't be used further. */
        if (i > 0) {
            close(pipes[(i-1)*2]); /* previous read end */
        }
        if (i < n-1) {
            close(pipes[i*2 + 1]); /* write end used by child; parent can close it */
        }

        /* Also close redirection fds in parent if set for first/last cmds */
        if (i == 0 && in_fd != STDIN_FILENO) close(in_fd);
        if (i == n-1 && out_fd != STDOUT_FILENO) close(out_fd);
    }

    /* parent: close any remaining pipe fds */
    for (int k = 0; k < pipes_count; ++k) {
        /* safe to close; some may already be closed */
        if (pipes[k*2] >= 0) close(pipes[k*2]);
        if (pipes[k*2+1] >= 0) close(pipes[k*2+1]);
    }

    free(pipes);

    if (!l->bg) {
        for (int i = 0; i < n; ++i) waitpid(pids[i], NULL, 0);
    } else {
        /* background: add first child as job */
        add_job(pids[0], l->seq[0][0], l->seq[0]);
    }

    free(pids);
    return EXIT_SUCCESS;
}