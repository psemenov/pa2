#include "common.h"
#include "ipc.h"
#include "pa2345.h"
#include "proc.h"
#include "banking.h"

#define ARGUMENTS_OFFSET 3

int proc_number;
balance_t balances[MAX_PROCESS_ID];


int get_arguments(int argc, char **argv);
Message init_msg(MessageType type , size_t payload_len);
void receive_all_msg(proc_t *proc, MessageType m_type);
void receive_all_balance(proc_t *proc, AllHistory *all_history);

