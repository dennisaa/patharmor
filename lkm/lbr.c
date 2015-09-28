#include <linux/sched.h>
#include <linux/scatterlist.h>
#include <linux/crypto.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/delay.h>
#include <linux/semaphore.h>

#include "armor.h"
#include "lbr.h"
#include "lbr-state.h"




struct lbr_t lbr_cache[LBR_CACHE_SIZE];


spinlock_t lbr_cache_lock;

#ifdef ARMOR_JIT
extern struct mutex validation_lock;
extern struct semaphore jit_work_cond;
extern struct semaphore jit_result_cond;
extern struct lbr_t *jit_work;
extern unsigned long jit_result;

extern struct jit_cache_t jit_cache;

extern unsigned long jit_waittime;

int disable_jit = 0;

#endif

/* this comes from userspace */
struct lbr_paths *paths = NULL;
    
/* global so that we have to allocate it only once */
struct hash_desc armor_desc;

extern struct armor_stats stats;


#if defined(ARMOR_DEBUG) || defined(ARMOR_DEBUG_JIT)
void cpuids(int *core, int *thread) {
    register int ebx asm("ebx");
    register int ecx asm("ecx");

    __asm__ ( "movl $1, %eax;"
              "cpuid;"
              "shr $24,%ebx;"
              "and $0xff, %ebx;" 
            
              "test $0x010000000,%edx;" 
              "jz ht_off;"
   
              "movl %ebx, %ecx;" 
              "and $1, %ecx;"
              "shr $1, %ebx;" 
              
              "jmp done;"        

      "ht_off: xor %ecx,%ecx;"
   
      "done:   ;");

    *core = ebx;
    *thread = ecx;
}
#endif

#ifdef ARMOR_DEBUG
inline void printd(bool print_cpuid, const char *fmt, ...) {
    int core_id, thread_id;
    va_list args;

    if (print_cpuid) cpuids(&core_id, &thread_id);

    va_start(args, fmt);
    if (print_cpuid) printk("A:%d:%d: ", core_id, thread_id);
    vprintk(fmt, args);
    va_end(args);
}
#else
inline void printd(bool print_cpuid, const char *fmt, ...) {
}
#endif

/* eeh.. */

#ifdef ARMOR_DEBUG_JIT
inline void printdj(bool print_cpuid, const char *fmt, ...) {
    int core_id, thread_id;
    va_list args;

    if (print_cpuid) cpuids(&core_id, &thread_id);

    va_start(args, fmt);
    if (print_cpuid) printk("A:%d:%d: ", core_id, thread_id);
    vprintk(fmt, args);
    va_end(args);
}

#else
inline void printdj(bool print_cpuid, const char *fmt, ...) {
}
#endif




/***********************************************************************
 * Helper functions for LBR related functionality.
 */

/* Flush the LBR registers. Caller should do get_cpu() and put_cpu().  */
void flush_lbr(bool enable) {
    int i;

    wrmsrl(MSR_LBR_TOS, 0);
    for (i = 0; i < LBR_ENTRIES; i++) {
        wrmsrl(MSR_LBR_NHM_FROM + i, 0);
        wrmsrl(MSR_LBR_NHM_TO   + i, 0);
    }
    if (enable) wrmsrl(MSR_IA32_DEBUGCTLMSR, IA32_DEBUGCTL);
    else        wrmsrl(MSR_IA32_DEBUGCTLMSR, 0);
}

/* Store the LBR registers for the current CPU into <lbr>. */
void get_lbr(struct lbr_t *lbr) {
    int i;

    rdmsrl(MSR_IA32_DEBUGCTLMSR, lbr->debug);
    rdmsrl(MSR_LBR_SELECT,       lbr->select);
    rdmsrl(MSR_LBR_TOS,          lbr->tos);
    for (i = 0; i < LBR_ENTRIES; i++) {
        rdmsrl(MSR_LBR_NHM_FROM + i, lbr->from[i]);
        rdmsrl(MSR_LBR_NHM_TO   + i, lbr->to  [i]);

        lbr->from[i] = LBR_FROM(lbr->from[i]);
    }
}

/* Write the LBR registers for the current CPU. */
void put_lbr(struct lbr_t *lbr) {
    int i;

    wrmsrl(MSR_IA32_DEBUGCTLMSR, lbr->debug);
    wrmsrl(MSR_LBR_SELECT,       lbr->select);
    wrmsrl(MSR_LBR_TOS,          lbr->tos);
    for (i = 0; i < LBR_ENTRIES; i++) {
        wrmsrl(MSR_LBR_NHM_FROM + i, lbr->from[i]);
        wrmsrl(MSR_LBR_NHM_TO   + i, lbr->to  [i]);
    }
}

/* Dump the LBR registers as stored in <lbr>. */
void dump_lbr(struct lbr_t *lbr) {
    int i;

    printd(true, "MSR_IA32_DEBUGCTLMSR: 0x%llx\n", lbr->debug);
    printd(true, "MSR_LBR_SELECT:       0x%llx\n", lbr->select);
    printd(true, "MSR_LBR_TOS:          %lld\n", lbr->tos);
    for (i = 0; i < LBR_ENTRIES; i++) {
      printd(true, "MSR_LBR_NHM_FROM[%2d]: 0x%llx\n", i, lbr->from[i]);
      printd(true, "MSR_LBR_NHM_TO  [%2d]: 0x%llx\n", i, lbr->to[i]);
    }
}

/* Initialize the LBR cache. */
void init_lbr_cache(void) {
    memset(&lbr_cache, 0, sizeof(lbr_cache));
}

/* Enable the LBR feature for the current CPU. *info may be NULL (it is required
 * by on_each_cpu()).
 */
void enable_lbr(void *info) {

    get_cpu();

    printd(true, "Enabling LBR\n");

    /* Apply the filter (what kind of branches do we want to track?) */
    wrmsrl(MSR_LBR_SELECT, LBR_SELECT);
    
    /* Flush the LBR and enable it */
    flush_lbr(true);

    put_cpu();
}



/******************************************************************************
 * Save and Restore the LBR state (during context switches). These should be as
 * fast as possible to minimize the overhead during context switches.
 */

/********* SAVE LBR */
void save_lbr(void) {
    int i;
    bool saved = false;
    unsigned long lbr_cache_flags;

    /* Multiple processes may be calling this function simultanously. We need to
     * lock a mutex so that no LBR stores are lost due to race conditions.
     */
    spin_lock_irqsave(&lbr_cache_lock, lbr_cache_flags);

    /* Loop over the LBR cache to find the first empty entry. */
    for (i = 0; i < LBR_CACHE_SIZE; i++) {
        if (lbr_cache[i].task == NULL) {
            
//          printd(true, "SAVE LBR - cache[%d] = %p (%s)\n", i, current, current->comm);

            /* Read the LBR registers into the LBR cache entry. */
            get_lbr(&lbr_cache[i]);

            /* Store our task_struct pointer in the LBR cache entry. */
            lbr_cache[i].task = current;

            saved = true;
            break;
        }
    }

    /* Crash if we were unable to save the LBR state. This could happen if too
     * many tasks (processes or threads) are running in parellel.
     */
    if (!saved) {
        printk("SAVE LBR - Purpose failed. LBR cache full?\n");
//      kill_pid(task_pid(current), SIGKILL, 1);
    }

    /* Disable the LBR for other processes. We don't care about the original LBR
     * state for now. Other modules also using the LBR should do their own
     * bookkeeping.
     *
     * One may argue that the LBR should be flushed here in order to avoid
     * information leakage about the target's ASLR state. The LBR registers can
     * only be accessed from within RING 0 though, so this is not really an
     * issue. 
     *
     * As an optimization step, we could decide to leave LBR debugging on. This
     * would save us a couple of instructions, but will slow down the entire
     * system.
     */
    wrmsrl(MSR_IA32_DEBUGCTLMSR, 0);

    /* Unlock the mutex. */
    spin_unlock_irqrestore(&lbr_cache_lock, lbr_cache_flags);
}

/********* RESTORE LBR */
void restore_lbr(void) {
    int i;
    bool found = false;
    unsigned long lbr_cache_flags;

    /* This mutex is here for safety only. I think it could be removed. */
    spin_lock_irqsave(&lbr_cache_lock, lbr_cache_flags);

    /* Loop over the LBR cache to find ourself. */
    for (i = 0; i < LBR_CACHE_SIZE; i++) {
        if (lbr_cache[i].task == current) {
//          printd(true, "LOAD LBR - cache[%d] = %p (%s)\n", i, current, current->comm);

            /* Write the LBR registers from the LBR cache entry. */
            put_lbr(&lbr_cache[i]);

            /* Clear the task_struct pointer so this entry can be reused again */
            lbr_cache[i].task = NULL;
            
            found = true;
            break;
        }
    }
    
    if (!found) {
        printk("LOAD LBR - Purpose failed for task %p (%s)\n", current, current->comm);

        /* This should never happen. All new tasks pass wake_up_new_task(), which
         * installs a LBR state into the cache.
         */
//      kill_pid(task_pid(current), SIGKILL, 1);
    }
    
    /* Unlock the mutex. */
    spin_unlock_irqrestore(&lbr_cache_lock, lbr_cache_flags);

}

    

/***********************************************************************
 * Two functions for hashing and validating the current LBR state.
 */

int hash_lbr(uint8_t hash[DIGEST_LENGTH],struct lbr_t *lbr) {
    struct scatterlist sg;
    int i, j;

    /* No error checking here. If anything fails, we better go straight home anyway. */

    crypto_hash_init(&armor_desc);
    armor_desc.flags = 0;

    /* Loop over all LBR entries. */
    for (i = 0; i < LBR_ENTRIES; i++) {
        sg_set_buf(&sg, &lbr->from[(lbr->tos - i) % LBR_ENTRIES], sizeof(uint64_t));
        crypto_hash_update(&armor_desc, &sg, sizeof(uint64_t));
        sg_set_buf(&sg, &lbr->to  [(lbr->tos - i) % LBR_ENTRIES], sizeof(uint64_t));
        crypto_hash_update(&armor_desc, &sg, sizeof(uint64_t));
        printdj(false, "lbr[%2d], <from: 0x%012llx, to: 0x%012llx>\n", i, 
                lbr->from[(lbr->tos+LBR_ENTRIES-i) % LBR_ENTRIES], 
                lbr->  to[(lbr->tos+LBR_ENTRIES-i) % LBR_ENTRIES]);
    }
    ARMOR_STAT_INC(digests);
    crypto_hash_final(&armor_desc, hash);
    
    printdj(false, "hash: ");
    for (j = 0; j < DIGEST_LENGTH; j++) printdj(false,"%02x", hash[j]);
    printdj(false,"\n");

    return 0;
}

void validate_lbr(void) {
#ifdef ARMOR_JIT
    uint8_t hash[DIGEST_LENGTH];
    int i;
    struct lbr_t lbr;
    struct timeval time;
    unsigned long jit_start_j, jit_stop_j, jit_delta_j;
    

    get_cpu();
    get_lbr(&lbr);
    dump_lbr(&lbr);
    put_cpu();

    if (disable_jit) {
        printk("[validation] -- WARNING -- JIT disabled!\n");
        return;
    }

    printdj(true,"[validation] JIT: Acquiring lock...\n");
    mutex_lock(&validation_lock);

    printdj(true,"[validation] JIT: Lookup\n");
    ARMOR_STAT_INC(jit_lookups);
    
    /* Compute a hash of the lbr and look it up in the cache. */
    hash_lbr(hash,&lbr);
    for (i = 0; i < jit_cache.hashes; i++) {
        if (memcmp(jit_cache.hash[i], hash, DIGEST_LENGTH) == 0) { 
            ARMOR_STAT_INC(jit_cache_hits);
#ifdef ARMOR_DEBUG_JIT
            printk("[validation] LBR state is valid (found in JIT cache)\n");
#endif
            mutex_unlock(&validation_lock);
            return;
        }
    }

    /* Not found in cache. Let's ask Dennis. Using Enes' semaphore design. */
    ARMOR_STAT_INC(jit_requests);
    jit_work = &lbr;

    printdj(true, "[validation] JIT: Request\n");

    /* Start the timers. */
    jit_start_j = jiffies;
    up(&jit_work_cond);

    printdj(true, "[validation] JIT: Waiting\n");
    if (down_timeout(&jit_result_cond, jit_waittime) < 0) {
        printk("[validation] JIT: Timeout\n");
        ARMOR_STAT_INC(jit_timeouts);
        disable_jit = 1;
        mutex_unlock(&validation_lock);
        return;
    }

    /* Stop the timers. */
    jit_stop_j = jiffies;
    jit_delta_j = jit_stop_j - jit_start_j;

    /* JIT may be faster than we can measure jiffies. If this happens, assume a
     * half jiffie was used.
     * http://stackoverflow.com/questions/10392735/am-i-too-fast-to-count-jiffies
     */
    if (jit_delta_j == 0) jit_delta_j = stats.jit_lookups % 2;
    printdj(true, "That took us %lu jiffies\n",     jit_delta_j);

            
    jiffies_to_timeval(jit_delta_j, &time);
    ARMOR_STAT_ADD(jit_sec, time.tv_sec);
    ARMOR_STAT_ADD(jit_usec, time.tv_usec);

    printdj(true, "[validation] JIT: Processing result\n");
    if (jit_result == 0) {
        printk("[validation] -- WARNING -- LBR state rendered *INVALID* by jit-analyzer\n");
        ARMOR_STAT_INC(jit_misses);
//      kill_pid(task_pid(current), SIGKILL, 1);
 
        printk("[validation] -- WARNING -- ASSUMING VALID\n");
        goto assume_valid;
    }
    if (jit_result == 2) {
        printk("[validation] -- WARNING -- LBR state not validated due to uninstrumentable function\n");
        ARMOR_STAT_INC(jit_unsupported);

        printk("[validation] -- WARNING -- ASSUMING VALID\n");
        goto assume_valid;
    }
    if (jit_result == 1) {
        ARMOR_STAT_INC(jit_hits);

assume_valid:
        /* Dennis' says it is ok. Let's add it to the - circular - cache so he
         * can take some time off next time. */
        /* TODO, this should probably be a sorted linked list so that we can do a binary search? */
        memcpy(jit_cache.hash[jit_cache.hashes], hash, DIGEST_LENGTH);
        jit_cache.hashes = (jit_cache.hashes + 1) % JIT_CACHE_SIZE;

#ifdef ARMOR_DEBUG_JIT
        printk("[validation] LBR state is valid\n");
#endif
    }

    mutex_unlock(&validation_lock);
    return;
#else
    struct lbr_t lbr;
    get_cpu();
    get_lbr(&lbr);
    dump_lbr(&lbr);
    put_cpu();
    return;
#endif // ARMOR_JIT


}




/***********************************************************************
 * Stack implementations.
 */
void stack_push(struct stack_t *s, uint64_t u) {
    if (s->size == STACKSIZE) {
        printk("[stack_push]: STACK OVERFLOW\n"); kill_pid(task_pid(current), SIGKILL, 1);
        return;
    }
    s->items[s->size] = u;
    s->size++;
    return;
}
uint64_t stack_pop(struct stack_t *s) {
    if (s->size == 0) {
        printk("[stack_pop ]: STACK UNDERFLOW\n"); kill_pid(task_pid(current), SIGKILL, 1);
        return 0;
    }
    s->size--;
    return s->items[s->size];
}
void lbr_stack_push(struct stack_lbr_t *s) {
    if (s->size == STACKSIZE) {
        printk("[lbr_stack_push]: STACK OVERFLOW\n"); kill_pid(task_pid(current), SIGKILL, 1);
        return;
    }
    get_cpu();
    get_lbr(&s->items[s->size]);
    put_cpu();
    s->size++;
}
void lbr_stack_pop(struct stack_lbr_t *s) {
    if (s->size == 0) {
        printk("[lbr_stack_pop ]: STACK UNDERFLOW\n"); kill_pid(task_pid(current), SIGKILL, 1);
        return;
    }
    s->size--;
    get_cpu();
    put_lbr(&s->items[s->size]); 
    put_cpu();
}
