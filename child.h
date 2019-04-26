#ifndef __IFMO_DISTRIBUTED_CLASS_PA1_CHILD_H__
#define __IFMO_DISTRIBUTED_CLASS_PA1_CHILD_H__

#include <stdio.h>
#include "ipc.h"
#include "io.h"
#include "banking.h"

typedef struct {
    const IO *io;       /**< Contrainter with I/O metadata. */
    local_id self_id;   /**< ID for the process. */
} proc_t;

/** Child main function.
 * 
 * @param p         Child process desciption.
 * @param history   Balance history of the process.
 */ 
int process_c(proc_t *p, balance_t balance);
void synchronize(proc_t *proc, MessageType m_type, char *payload, size_t payload_len);
void balance_copy(BalanceHistory *balance_history, uint8_t time);
void balance_set(BalanceHistory *balance_history, balance_t balance);
void transfer_cycle(proc_t *proc, BalanceHistory *balance_history, TransferOrder *transfer_order);
void working_cycle(proc_t *proc, BalanceHistory *balance_history);

#endif /* __IFMO_DISTRIBUTED_CLASS_PA1_CHILD_H__ */
