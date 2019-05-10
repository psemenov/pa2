#include "common.h"
#include "ipc.h"
#include "pa2345.h"
#include "child.h"
#include "banking.h"

#define ARGUMENTS_OFFSET 3

Message init_msg(MessageType type , size_t payload_len);

