#ifndef __LBR_H__
#define __LBR_H__

#include <linux/sched.h>
#include "lbr-state.h"

/* To enable the LBR capabilities of recent Intel processors, one has to 
 * write specific values to two model specific registers (MSR):
 * - MSR_IA32_DEBUGCTL: To enable the LBR feature in the first place.
 * - MSR_LBR_SELECT:    To tell the CPU what kind of branches it should track.
 */

#ifndef MSR_IA32_DEBUGCTLMSR
#define MSR_IA32_DEBUGCTLMSR        0x000001d9
#endif
#ifndef MSR_LBR_SELECT
#define MSR_LBR_SELECT              0x000001c8
#endif
#ifndef MSR_LBR_TOS
#define MSR_LBR_TOS                 0x000001c9
#endif
#ifndef MSR_LBR_NHM_FROM
#define MSR_LBR_NHM_FROM            0x00000680
#endif
#ifndef MSR_LBR_NHM_TO
#define MSR_LBR_NHM_TO              0x000006c0
#endif

/* The lowest bit in the MSR_IA32_DEBUG register enables the LBR feature */
#define IA32_DEBUGCTL 0x1

/* The LBR_SELECT register has the following options:
 *
 * Bit Field     Bit Offset Access Description
 * CPL_EQ_0      0          R/W    When set, do not capture branches occurring in ring 0
 * CPL_NEQ_0     1          R/W    When set, do not capture branches occurring in ring >0
 * JCC           2          R/W    When set, do not capture conditional branches
 * NEAR_REL_CALL 3          R/W    When set, do not capture near relative calls
 * NEAR_IND_CALL 4          R/W    When set, do not capture near indirect calls
 * NEAR_RET      5          R/W    When set, do not capture near returns
 * NEAR_IND_JMP  6          R/W    When set, do not capture near indirect jumps
 * NEAR_REL_JMP  7          R/W    When set, do not capture near relative jumps
 * FAR_BRANCH    8          R/W    When set, do not capture far branches
 * EN_CALLSTACK  9                 Enable LBR stack to use LIFO filtering to capture
 *                                 call stack profile
 * Reserved      63:10             Must be zero
 *
 * This is currently set to:
 * 0x185 = 0110000101   --> capture branches occuring in ring >0;
 *                          capture near relative calls
 *                          capture near indirect calls
 *                          capture near returns
 *                          capture near indirect jumps
 */
#define LBR_SELECT 0x185

/* This is the number of LBR states that we allocate room for in our module.
 * Whenever a process that is being LBRed is scheduled out, we store its LBR
 * state into a cache. This number thus represents the maximum number of
 * simultanously active threads/processes we support for our target.
 */
#define LBR_CACHE_SIZE 1024


/* The LBR.from registers contain a bit more information than just the source
 * address of the branch:
 *
 * Bit Field Bit Offset Access Description   
 * Data      47:0       R/O    The linear address of the branch instruction itself, 
 *                             this is the “branch from“ address.
 * SIGN_EXT  60:48      R/O    Signed extension of bit 47 of this register.
 * TSX_ABORT 61         R/O    When set, indicates a TSX Abort entry 
 *                             LBR_FROM: EIP at the time of the TSX Abort
 *                             LBR_TO  : EIP of the start of HLE region, or EIP of 
 *                                       the RTM Abort Handler
 * IN_TSX    62         R/O    When set, indicates the entry occurred in a TSX region
 * MISPRED   63         R/O    When set, indicates either the target of the branch was 
 *                             mispredicted and/or the direction (taken/non-taken) was 
 *                             mispredicted; otherwise, the target branch was predicted.
 *
 * The layout of _TO registers is simpler:
 *
 * Bit Field Bit Offset Access Description
 * Data      47:0       R/O    The linear address of the target of the branch instruction i
 *                             itself, this is the “branch to“ address.
 * SIGN_EXT  63:4       R/O    Signed extension of bit 47 of this register.
 */
#define LBR_FROM_FLAG_MISPRED (1ULL << 63)
#define LBR_FROM_FLAG_IN_TSX  (1ULL << 62)
#define LBR_FROM_FLAG_ABORT   (1ULL << 61)
/* I would say that we can ignore above and merge Data with SIGN_EXT */
#define LBR_SKIP 3
#define LBR_FROM(from) (uint64_t)((((int64_t)from) << LBR_SKIP) >> LBR_SKIP)



/* I am currently using a bit in (unsigned int) task_struct->personality to mark
 * a task as being 'LBRed'. This seems to be safe as the top 4 bits of this int
 * are currently not used (see include/uapi/linux/personality.h).
 *
 * This value is retained during task copies. This means that by setting it
 * once, right before our target is started, all future target's children and
 * (kernel) threads will have this bit set as well.
 *
 * We do not support user space threads.
 */
#define TASK_LBR 0x80000000
#define SUSP_LBR 0x40000000

/* inline functions to set, clear and test a task for the 'LBR' bit */
static inline void set_task_lbr(struct task_struct *task) { task->personality |=  TASK_LBR; }
static inline void clr_task_lbr(struct task_struct *task) { task->personality &= ~TASK_LBR; }
static inline  int tst_task_lbr(struct task_struct *task) { return task->personality & TASK_LBR; }

static inline void   susp_task_lbr(struct task_struct *task) { task->personality &= ~TASK_LBR;
                                                               task->personality |=  SUSP_LBR; }
static inline void wakeup_task_lbr(struct task_struct *task) { task->personality &= ~SUSP_LBR;
                                                               task->personality |=  TASK_LBR; }
static inline int  issusp_task_lbr(struct task_struct *task) { return task->personality & SUSP_LBR; }


/* This struct is used to hold the LBR registers for a CPU on a per-task base */
#define IA32_DEBUGCTL 0x1

/* When our kernel module receives a LIB_ENTER or CBK_ENTER request, we need to
 * store the provided return address and return this upon LIB_EXIT or CBK_EXIT
 * ioctls. We store these return addressess per task in a stack data structure:
 * a task may make multiple subsequent LIB_ENTER ioctls (e.g., LIB_ENTER ->
 * CBK_ENTER -> LIB_ENTER) and we thus always need to return the last saved 
 * return value.
 *
 * We use three data structures.
 * - library enter/exit
 * - callback enter/exit
 * - signal enter/exit
 * 
 * To avoid extra costs of using kmalloc() and kfree(), we use a statically
 * allocated array to represent the stack data structures.
 */
#define STACKSIZE 8

/* This is our stack data structure for return values. */
struct stack_t {
    size_t size;
    uint64_t items[STACKSIZE]; // only support for uint64_t values (return addresses)
};
    void stack_push(struct stack_t *s, uint64_t u);
uint64_t stack_pop (struct stack_t *s);
uint64_t stack_read(struct stack_t *s);     // read the top of the stack without popping it

/* This is our stack data structure for LBR contents. */
struct stack_lbr_t {
    size_t size;
    struct lbr_t items[STACKSIZE];
};
void lbr_stack_push(struct stack_lbr_t *s); // will read from the LBR directly
void lbr_stack_pop (struct stack_lbr_t *s); // will write to the LBR directly

/* The following cache sizes reflect the maximum number of target
 * threads/processes that are allowed to make library calls, callback calls or
 * signal handler calls in parallel (i.e., thread A is executing a library
 * function, while thread B is also entering such function).
 */
#define LIB_CACHE_SIZE 1024
#define CBK_CACHE_SIZE 64
#define SIG_CACHE_SIZE 64

/* This is the array structure that holds library/callback/signal information on
 * a per-task basis. Not all fields are used by all different types, but
 * combining them into one struct makes things a bit easier code-wise.
 */
struct wrapper_lib_t {
    struct task_struct *task;
    struct stack_t      rets;
    /* When entering a callback function, the LBR will be flushed. We thus need
     * to store the current contents of the LBR so that we can restore it upon
     * CBK_EXIT. */
    struct stack_lbr_t  lbrs;
};


#define JIT_CACHE_SIZE 512

/* window size of 16 for the jit cache. */
struct jit_cache_t {
    uint8_t hash[JIT_CACHE_SIZE][DIGEST_LENGTH];
    uint64_t hashes;
};



#ifdef ARMOR_STATS
#define ARMOR_STAT(S)       (stats.S)
#define ARMOR_STAT_INC(S)   (ARMOR_STAT(S)++)
#define ARMOR_STAT_ADD(S,y) (ARMOR_STAT(S)+=y)
#define ARMOR_STAT_DEC(S)   (ARMOR_STAT(S)--)
#else
#define ARMOR_STAT_INC(s) { }
#define ARMOR_STAT_DEC(s) { }
#endif

struct armor_stats {
    uint64_t sig_enter;
    uint64_t sig_return;
    uint64_t lib_enter;
    uint64_t lib_return;
    uint64_t cbk_enter;
    uint64_t cbk_return;

    uint64_t new_tasks;
    uint64_t tasks_exit;

    // dangerous system calls
    uint64_t execve;
    uint64_t mmap;
    uint64_t mmap_exec;
    uint64_t read;
    uint64_t write;
    uint64_t mprotect;
    uint64_t mprotect_exec;
    uint64_t sigaction;

    // context switches
    uint64_t sched_out;
    uint64_t sched_in;

    // number of hashes computed
    uint64_t digests;

    uint64_t offline_lookups; 
    uint64_t offline_hits;
    uint64_t offline_misses;

    uint64_t jit_lookups;
    uint64_t jit_cache_hits;
    uint64_t jit_requests;
    uint64_t jit_hits;
    uint64_t jit_unsupported;
    uint64_t jit_misses;
    uint64_t jit_busy;
    uint64_t jit_timeouts;

    uint64_t offline_cache_size;
    uint64_t jit_cache_size;

    uint64_t jit_sec;
    uint64_t jit_usec;
};








/***** Function prototypes *****/

/* Initialize the LBR cache (memset to 0) */
void init_lbr_cache(void);

/* Fetch the current LBR state and store it into our cache (during sched_out) */
void save_lbr(void);
/* Search our cache for the LBR state for the current active task and restore */
void restore_lbr(void);


/* Enable the LBR feature for the current CPU */
void enable_lbr(void* info);

void flush_lbr(bool enable);

/* Read the contents of the LBR registers into a lbr_t structure */
void get_lbr(struct lbr_t *lbr);
/* Write the contents of a lbr_t structure into the LBR registers */
void put_lbr(struct lbr_t *lbr);

/* Dump the contents of a lbr_t structure to syslog */
void dump_lbr(struct lbr_t *lbr);

/* Compute hashes for the current state of the LBR */
int hash_lbr(uint8_t hash[DIGEST_LENGTH], struct lbr_t *lbr);

/* Validate the LBR */
void validate_lbr(void);

inline void printd(bool print_cpuid, const char *fmt, ...);
    
#endif /* __LBR_H__ */
