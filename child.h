#ifndef __IFMO_DISTRIBUTED_CLASS_PA1_CHILD_H__
#define __IFMO_DISTRIBUTED_CLASS_PA1_CHILD_H__

#include <stdio.h>
#include "ipc.h"
#include "io.h"
#include "banking.h"

typedef struct {
    const IO *io;       /**< Contrainter with I/O metadata. */
    local_id self_id;   /**< ID for the process. */
    local_id procnum;
} proc_t;

/** Child main function.
 * 
 * @param p         Child process desciption.
 * @param history   Balance history of the process.
 */ 
int child(proc_t *p, balance_t balance);

#endif /* __IFMO_DISTRIBUTED_CLASS_PA1_CHILD_H__ */
