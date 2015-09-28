#pragma once
#define DEBUG
//in fact that the size should not longer than 16, however, some of them can be 17
#define MAX_RAW_INSN_SIZE 32
#define BUFFER_STRING_LEN 1024
#define FS_SEGMENT_REG_VAL 0x8000
#define MAX_NUM_PREFIXS 13
#define REX_DEFAULT_PATTERN 0x40

#define NR_REGS 17
#define FLAG_WRITE 0x10
#define FLAG_READ 0x1
