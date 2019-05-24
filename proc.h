#include <stdio.h>
#include "ipc.h"
#include "banking.h"
#include "common.h"
#include "pa2345.h"

#define ARR_SIZE    11
#define FD_MAX      2
#define READ_FD     0
#define WRITE_FD    1

#define ARGUMENTS_OFFSET 3

int proc_number;
balance_t balances[MAX_PROCESS_ID];
timestamp_t lamport_time;

FILE *event_log;
FILE *pipe_log;
int pipes[ARR_SIZE][ARR_SIZE][FD_MAX];

int init_pipes(int pipes[ARR_SIZE][ARR_SIZE][FD_MAX]);
void close_fds(int pipes[ARR_SIZE][ARR_SIZE][FD_MAX], local_id id);

typedef struct {
    local_id id;   /**< ID for the process. */
} process;

int get_arguments(int argc, char **argv);
Message init_msg(MessageType type , size_t payload_len);
void receive_all_msg(process *proc, MessageType m_type);
void receive_all_balance(process *proc, AllHistory *all_history);

int process_c(process *p, balance_t balance);
void synchronize(process *proc, MessageType m_type, char *payload, size_t payload_len);
void balance_copy(BalanceHistory *balance_history, uint8_t time);
void balance_set(BalanceHistory *balance_history, balance_t balance);
void transfer_cycle(process *proc, BalanceHistory *balance_history, TransferOrder *transfer_order);
void working_cycle(process *proc, BalanceHistory *balance_history);

/* lamport */
timestamp_t get_lamport_time();
void set_lamport_time(timestamp_t time);
void inc_time();
