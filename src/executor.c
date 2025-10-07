#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/wait.h> //added for wait() call
#include <sys/types.h> //for pid
#include <string.h>

#include "executor.h"


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