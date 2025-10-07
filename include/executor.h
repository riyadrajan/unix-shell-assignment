//
// Created by Paula on 2025-08-11.
//

#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"
#include <sys/types.h>

int execute_command(char* cmd, char** args, int in, int out, int bg);

int execute(struct cmdline *l);

// use a structure to represent the list of jobs (since they can hold the attributes to print)
//took syntax from w3 schools
typedef struct {
    int job_id;        // like 1, 2, 3, ... to display running jobs
    pid_t pid;         // pid of the background command
    enum {Running, Stopped, Done} state;   // current status (Running/Stopped/Done)
    char *cmdstr; // the command string
} job_t;


void add_job(pid_t pid, char* cmd, char** args);
void cleanup_finished_jobs(void);
void print_jobs(void);
int wr_to_file(struct cmdline *l);
int rd_from_file(struct cmdline *l);
#endif //EXECUTOR_H