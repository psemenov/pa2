#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>


#include "proc.h"

int get_arguments(int argc, char **argv) {
    if (argc != atoi(argv[2])+ARGUMENTS_OFFSET || (strcmp(argv[1], "-p") != 0))     return -1;
    proc_number = atoi(argv[2]);
    if(proc_number < 0) return -1;
    for (int i = 0; i < proc_number; i++){
        balances[i] = atoi(argv[i+ARGUMENTS_OFFSET]);
        if (balances[i] < 0){ 
            return -1;
        }
    }
    return 0;
}


timestamp_t get_lamport_time() {
    return lamport_time;
}

void set_lamport_time(timestamp_t time) {
    if (time > lamport_time) lamport_time = time;
}

void inc_time() {
    lamport_time++;
}

int init_pipes(int pipes[ARR_SIZE][ARR_SIZE][FD_MAX]) {
    int count = 1;
    for (int i = 0; i <= proc_number; i++) {
        for (int j = 0; j <= proc_number; j++) {
            if (i == j) {
                pipes[i][j][READ_FD]  = -1;
                pipes[i][j][WRITE_FD] = -1;
                continue;
            }
            if (pipe2(pipes[i][j], O_NONBLOCK | O_DIRECT) < 0) {
               perror("pipe:init_pipes()");
               return -1;
            }
            fprintf(pipe_log, "Pipe with number %d was created.\n", count++);

        }
    }
    return 0;
}

void close_fds(int pipes[ARR_SIZE][ARR_SIZE][FD_MAX], local_id id) {
    for (local_id i = 0; i <= proc_number; i++) {
        for (local_id j = 0; j <= proc_number; j++) {
            if (i != j) {
                if (i == id) {
                    close(pipes[i][j][READ_FD]);
                    fprintf(pipe_log, "PID:%d closed read(%hhd -- %hhd).\n", id, i,j);
                }
                if (j == id) {
                    close(pipes[i][j][WRITE_FD]);
                    fprintf(pipe_log, "PID:%d closed write(%hhd -- %hhd).\n", id, i,j);
                }
                if (i != id && j != id) {
                    fprintf(pipe_log, "PID:%d closed pipe(%hhd -- %hhd).\n", id, i,j);
                    close(pipes[i][j][WRITE_FD]);
                    close(pipes[i][j][READ_FD]);
                }
            }
        }
    }
    fprintf(pipe_log, "PID:%d closed all fds.\n", id);
}


Message init_msg(MessageType type , size_t payload_len){
    Message msg;
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = type;
    msg.s_header.s_payload_len = payload_len;
    msg.s_header.s_local_time = get_lamport_time();
    return msg;
}

void receive_all_msg(process *proc, MessageType m_type) {
    Message tmp_msg = { {0} };
    for (local_id i = 1; i <= proc_number; i++) {
        while(receive((void*)proc, i, &tmp_msg) != 0);
        set_lamport_time(tmp_msg.s_header.s_local_time);
        inc_time();
        if (tmp_msg.s_header.s_type != m_type) {
            fprintf(pipe_log, 
                    "Wrong message received: expected %d,instead got %d.\n",
                    m_type, tmp_msg.s_header.s_type);
        }
    }
}


void receive_all_balance(process *proc, AllHistory *all_history) {
    Message tmp_msg = {{ 0 }};
    local_id i = 1;
    while (i <= proc_number) {
        while(receive((void*)proc, i, &tmp_msg) != 0)
            ;
            set_lamport_time(tmp_msg.s_header.s_local_time);
            inc_time();

            if (tmp_msg.s_header.s_type == BALANCE_HISTORY) {
                memcpy(&all_history->s_history[i-1], &tmp_msg.s_payload, tmp_msg.s_header.s_payload_len);
                balance_t b  = all_history->s_history[i-1].s_history[all_history->s_history[i-1].s_history_len].s_balance;
                fprintf(event_log, log_done_fmt, get_lamport_time(),proc->id, b);
                i++;
            }
    }
    all_history->s_history_len = proc_number;
}

int main(int argc, char *argv[]) {

    process proc;

    if(get_arguments(argc, argv) != 0) {
        perror("Wrong arguments:get_arguments() main.c");
        return -1;
    }

    event_log = fopen(events_log, "w+");
    pipe_log = fopen(pipes_log, "w");

    if (init_pipes(pipes) < 0)
        return -1;

    for (int i = 1; i <= proc_number; i++) {
        pid_t pid = fork();
        if (0 > pid) {
            exit(EXIT_FAILURE);
        } else if (0 == pid) {
            /* Child. */
            proc.id = i;
            int ret = process_c(&proc, balances[i-1]);
            exit(ret);
        }
    }
    close_fds(pipes, PARENT_ID);

    proc.id = PARENT_ID;
    AllHistory all_history = { 0 };

    receive_all_msg(&proc, STARTED);
    fprintf(event_log, log_received_all_started_fmt, 
            get_lamport_time(), PARENT_ID);

    bank_robbery(&proc, proc_number);

    inc_time();
    Message msg = init_msg(STOP,0);
    send_multicast(&proc,&msg);

    receive_all_msg(&proc, DONE);
    fprintf(event_log,log_received_all_done_fmt,
            get_lamport_time(), PARENT_ID);
    receive_all_balance(&proc, &all_history);
    print_history(&all_history);

    while(wait(NULL) > 0);

    fclose(pipe_log);
    fclose(event_log);
    return 0;
}
