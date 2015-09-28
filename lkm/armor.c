#include <linux/compat.h>
#include <linux/crypto.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/miscdevice.h>
#include <linux/mman.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/sysctl.h>

#include <linux/spinlock.h>
#include <linux/spinlock_types.h>

#include "armor.h"
#include "lbr.h"
#include "intercept.h"
#include "lbr-state.h"

extern void pre_signal_delivered(void);
extern void pre_signal_delivered_exit(void);

/* MICRO BENCHMARK */
#ifndef ARMOR_VALIDATE
#define validate_lbr() { }
#endif


MODULE_AUTHOR("Victor van der Veen");
MODULE_DESCRIPTION("Armor LBR Module");
MODULE_LICENSE("GPL");

/* External variables defined in lbr.c. */
extern struct lbr_t lbr_cache[LBR_CACHE_SIZE];
extern struct lbr_paths *paths;

#ifdef ARMOR_STATS
static struct ctl_table_header *sysctl_header;
#endif

static struct workqueue_struct *exit_queue;

typedef struct exit_work_s {
    struct work_struct work;
    struct task_struct *task;
} exit_work_t;



struct lbr_paths_kernel_copy tocopy;

extern struct hash_desc armor_desc;

struct task_struct *main_task;

int armor_initialized = 0;
int armor_opens = 0;
extern int disable_jit;
    
/***********************************************************
 * INIT
 *   work_cond = 0
 * result_cond = 0
 *
 *
 *
 * VALIDATION                                   IOCTL INTERFACE
 *
 * lock(validation_lock)
 *
 * work = x                                     down(work_cond) // wait for work
 *
 * up(work_cond)        // we have work
 * down(result_cond)    // wait for the result
 *                                              result = process(work)
 *                                              up(result_cond) // we have the result
 * process(result)
 * unlock(validation_lock)
 * 
 *
 */
#ifdef ARMOR_JIT
struct mutex validation_lock;
struct semaphore jit_work_cond;
struct semaphore jit_result_cond;
struct lbr_t *jit_work;
unsigned long jit_result;

struct jit_cache_t jit_cache;

unsigned long jit_waittime;
#endif

struct plt_got_copy_t plt_got_copy;
struct simples_t simples;

struct semaphore target_started;
struct semaphore plt_available;
struct semaphore exitpoints_available;




/* Cache arrays and mutexes to store and handle return addresses and LBR states
 * during library/callback/signal invocations.
 */
struct wrapper_lib_t lib_data[LIB_CACHE_SIZE], 
                     cbk_data[CBK_CACHE_SIZE], 
                     sig_data[SIG_CACHE_SIZE];
struct mutex lib_data_mutex, 
             cbk_data_mutex, 
             sig_data_mutex;

//extern struct mutex lbr_cache_mutex;
extern spinlock_t lbr_cache_lock;
extern unsigned long lbr_cache_flags;

struct armor_stats stats;


/******************************************************************************
 * Hooks on context switches.
 */
static void sched_in(struct preempt_notifier *pn, int cpu) {
    ARMOR_STAT_INC(sched_in);
    restore_lbr();
}
static void sched_out(struct preempt_notifier *pn, struct task_struct *next) {
    ARMOR_STAT_INC(sched_out);
    save_lbr();
}

/* Structures for installing the context switch hooks. */
struct preempt_notifier notifier;
struct preempt_ops ops = {
    .sched_in  = sched_in,
    .sched_out = sched_out
};


#ifdef ARMOR_STATS
static struct ctl_table armor_stats_table[] = {
    { .procname = "stats_sig_enter",  .data = &ARMOR_STAT(sig_enter),  .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_sig_return", .data = &ARMOR_STAT(sig_return), .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_lib_enter",  .data = &ARMOR_STAT(lib_enter),  .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_lib_return", .data = &ARMOR_STAT(lib_return), .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_cbk_enter",  .data = &ARMOR_STAT(cbk_enter),  .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_cbk_return", .data = &ARMOR_STAT(cbk_return), .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },

    { .procname = "stats_new_tasks",  .data = &ARMOR_STAT(new_tasks),  .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_tasks_exit", .data = &ARMOR_STAT(tasks_exit), .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    
    { .procname = "stats_execve",        .data = &ARMOR_STAT(execve),        .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_mmap",          .data = &ARMOR_STAT(mmap),          .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_mmap_exec",     .data = &ARMOR_STAT(mmap_exec),     .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_read",          .data = &ARMOR_STAT(read),      .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_write",          .data = &ARMOR_STAT(write),      .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_mprotect",      .data = &ARMOR_STAT(mprotect),      .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_mprotect_exec", .data = &ARMOR_STAT(mprotect_exec), .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_sigaction",     .data = &ARMOR_STAT(sigaction),     .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },

    { .procname = "stats_sched_out", .data = &ARMOR_STAT(sched_out), .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_sched_in",  .data = &ARMOR_STAT(sched_in),  .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    
    { .procname = "stats_digests",  .data = &ARMOR_STAT(digests), .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },

    { .procname = "stats_jit_cache_size", .data = &ARMOR_STAT(jit_cache_size), .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },


/*** VALIDATION STATS ***/
    /* Number of JIT lookups. */
    { .procname = "stats_jit_lookups",  .data = &ARMOR_STAT(jit_lookups), .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    
    /* Number of JIT cache hits. These are paths for which we did JIT analysis once, but then got cached and executed more often. */
    { .procname = "stats_jit_cache_hits",  .data = &ARMOR_STAT(jit_cache_hits), .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },

    /* Number of JIT requests. This is how many times we need to ask Dennis for an answer. */ 
    { .procname = "stats_jit_requests", .data = &ARMOR_STAT(jit_requests),  .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },

    /* Amount of time spent in Dennis' JIT code. */
    { .procname = "stats_jit_sec",   .data = &ARMOR_STAT(jit_sec),   .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_jit_usec",  .data = &ARMOR_STAT(jit_usec),  .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },

    /* Number of JIT hits and misses: number of times Dennis says the LBR is ok or not. */
    { .procname = "stats_jit_hits",    .data = &ARMOR_STAT(jit_hits),    .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_jit_misses",  .data = &ARMOR_STAT(jit_misses),  .maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    { .procname = "stats_jit_timeouts",.data = &ARMOR_STAT(jit_timeouts),.maxlen = sizeof(int), .mode = 0666, .proc_handler = proc_dointvec, },
    
    { }
};

static struct ctl_table armor_table[] = {
    { .procname = "stats", .mode = 0555, .child = armor_stats_table, },
    { }
};

static struct ctl_table armor_dir[] = {
    { .procname = "patharmor", .mode = 0555, .child = armor_table, },
    { }
};
#endif




/******************************************************************************
 * Helper functions for handling library, callback and signal entry/exit
 * occurences.
 *
 * These should be as fast as possible to limit runtime overhead.
 */

/* Search one of the library/callback/signal cache arrays for the entry that
 * belongs to the current task. Add a new entry if no one was found.
 * Multiple threads may be banging on this function when entering library calls
 * in parallel. The caller should request a mutex lock so that there is no race
 * condition on tmp entry possible.
 */
static inline int find_cache_entry_init(struct wrapper_lib_t *data, 
                                           int                cache_size, 
                                        struct task_struct   *task) {
    int i;
    int tmp = -1;
    for (i = 0; i < cache_size; i++) {
        if (data[i].task == task) return i;
        if (data[i].task == NULL && tmp == -1) tmp = i; /* Remember the first empty entry. */
    }
    if (tmp == -1) {
        printk("Could not find empty cache entry\n");
        kill_pid(task_pid(current), SIGKILL, 1);

        /* By returning 0, the caller will use the wrong cache entry. We already
         * segfaulted the target though, so we are about to be done soon.
         * This removes an extra return value check by the caller.
         */
        return 0; 
    }
    /* Fill up a new entry. */
    data[tmp].task = task;
    return tmp;
}
static inline void lib_init(uint64_t ret) {
    /* Search an entry in the lib cache where we can store the return address. */
    int i;
    mutex_lock(&lib_data_mutex);
    i = find_cache_entry_init(lib_data, LIB_CACHE_SIZE, current);
    mutex_unlock(&lib_data_mutex);

    /* Add the return address to this entry's stack of return addresses so that
     * it is the first that will be popped when we exit the library function.
     */
    stack_push(&lib_data[i].rets, ret);

}
static inline void cbk_init(uint64_t ret) {
    /* Search an entry in the callback cache where we can store the return
     * address and LBR state. */
    int i;
    mutex_lock(&cbk_data_mutex);
    i = find_cache_entry_init(cbk_data, CBK_CACHE_SIZE, current);
    mutex_unlock(&cbk_data_mutex);

    /* Add the return address t this entry's stack of return addresses. */
    stack_push(&cbk_data[i].rets, ret);

    /* Add the current LBR state to this entry's stack of LBR states. */
    lbr_stack_push(&cbk_data[i].lbrs);

    /* Flush the current LBR and enable it. */
    get_cpu(); flush_lbr(true); put_cpu();
}
static inline void sig_init(uint64_t sighandler) {
    /* Search an entry in the signal cache where we can store the LBR state. */
    int i;
    mutex_lock(&sig_data_mutex);
    i = find_cache_entry_init(sig_data, SIG_CACHE_SIZE, current);
    mutex_unlock(&sig_data_mutex);
    
    /* Add the current LBR state to this entry's stack of LBR states. */
    lbr_stack_push(&sig_data[i].lbrs);

    /* Flush the current LBR and enable it if we are calling a target signal handler. */
    get_cpu();
    if (sighandler > (uint64_t)current->mm->start_code &&
        sighandler < (uint64_t)current->mm->end_code)       flush_lbr(true);
    else                                                    flush_lbr(false);

    /* Write the first LBR entry. */
    wrmsrl(MSR_LBR_NHM_FROM, tocopy.indirect_call_source);
    wrmsrl(MSR_LBR_NHM_TO,   sighandler);
    wrmsrl(MSR_LBR_TOS, 0);
    put_cpu();
}

static inline uint64_t lib_exit(void) {
    /* Find the cache entry for the current task and reset it if it has only one element.  */

    int i;
    for (i = 0; i < LIB_CACHE_SIZE; i++) {
        if (lib_data[i].task == current) {
            if (lib_data[i].rets.size == 1) {
                lib_data[i].task = NULL;
            }
            return stack_pop(&lib_data[i].rets);
        }
    }
    printk("[lib_exit] Could not find cache entry\n");
    kill_pid(task_pid(current), SIGKILL, 1);
    /* By returning 0, we overwrite the return address with a NULL pointer. We
     * already segfaulted the target though, so we are about to be done soon.
     */
    return 0;
}
static inline uint64_t cbk_exit(void) {
    /* Same as lib_exit, but also restore the LBR state. */
    int i;
    for (i = 0; i < CBK_CACHE_SIZE; i++) {
        if (cbk_data[i].task == current) {
            if (cbk_data[i].rets.size == 1) {
                cbk_data[i].task = NULL;
            }
               lbr_stack_pop(&cbk_data[i].lbrs);
            return stack_pop(&cbk_data[i].rets);
        }
    }
    printk("[cbk_exit] Could not find cache entry\n");
    kill_pid(task_pid(current), SIGKILL, 1);
    /* By returning 0, we overwrite the return address with a NULL pointer. We
     * already segfaulted the target though, so we are about to be done soon.
     */
    return 0;
}
static inline void sig_exit(void) {
    int i;
    /* Same as cbk_exit, but without restoring the retun address. */
    for (i = 0; i < SIG_CACHE_SIZE; i++) {
        if (sig_data[i].task == current) {
            if (sig_data[i].lbrs.size == 1) {
                sig_data[i].task = NULL;
            }
            lbr_stack_pop(&sig_data[i].lbrs);
            return;
        }
    }
    printk("[sig_exit] Could not find cache entry\n");
    kill_pid(task_pid(current), SIGKILL, 1);
    return;
}



/******************************************************************************
 * Overloaded kernel functions to hook on signal events.
 */

/******** SIGNAL ENTER */
void new_signal_delivered(int sig, siginfo_t *info, struct k_sigaction *ka,
                struct pt_regs *regs, int stepping) {

    /* Only continue if this task is under our supervision. */
    if (tst_task_lbr(current)) {

        /* Only continue if a signal handler is installed. */
        if (ka->sa.sa_handler != NULL) {
            ARMOR_STAT_INC(sig_enter);
            
            printd(true,"SIG_ENTER. Starting validation process\n");
            validate_lbr();

            printd(true, "SIG_ENTER to handler @ %p\n", ka->sa.sa_handler);
           
            /* This will store the current task and LBR state in the 'signal
             * cache' so that we can restore it during sigreturn. Depending on
             * whether the signal handler address relies in the target, this
             * also flushes and enables the LBR.
             * Note that we always store and restore LBR states during signals,
             * and not only when signals are handler by target code. This is due
             * to our sigreturn hook that cannot determine wether we returned
             * from target code or not.
             */
            sig_init((uint64_t) ka->sa.sa_handler);
        }
    }
   
    /* Continue to the real signal_delivered() implementation (in kernel/signal.c). */
}

/******** SIGNAL EXIT */
void new_sys_rt_sigreturn(void) {
    /* Only continue if this task is under our supervision. */
    if (tst_task_lbr(current)) {
        ARMOR_STAT_INC(sig_return);

        /* This will restore the LBR state to the state it was right before the
         * signal was delivered, this includes the bit that says whether it is
         * enabled or disabled: we have currently no way of telling where we are
         * returning into.
         */
        sig_exit();
    }

    /* Continue to the real sys_rt_sigreturn() implementation . */
}
 


/******************************************************************************
 * Overloaded kernel functions to handle process/thread creation and removal. 
 */

/******** task creation */
long new_wake_up_new_task(struct task_struct *p) {
    int i, new_index, our_index;
    unsigned long lbr_cache_flags;

    /* Only continue if the new task is under our supervision. */
    if (tst_task_lbr(p)) {
        ARMOR_STAT_INC(new_tasks);
        printd(true, "WAKE UP! - New task @ 0x%p\n",p);

        /* Install the hooks on context switches for this task so we can save
         * and restore its LBR state whenever it is scheduled in/out. We can
         * reuse the notifier struct for this that was initialized during
         * ARMOR_IOC_INIT.
         */
        hlist_add_head(&notifier.link, &p->preempt_notifiers);

        /* This task could be the result of a fork-like function call. When such
         * call finishes, we will will need a return address. Just to be safe,
         * we copy the entire 'lib cache' (if there is any) from the current
         * task to the new one.
         * 
         * If the task is not the result of a fork-like function - like with
         * pthread_create() - copying the 'lib cache' should cause no harm. At
         * most, there will be one or two return addresses on the stack that
         * will never be popped.
         *
         * Let's start with finding our own entry. If there is no such entry, we
         * can skip copying the library cache.
         */
        our_index = -1;
        for (i = 0; i < LIB_CACHE_SIZE; i++) {
            if (lib_data[i].task == current) {
                our_index = i;
            }
        }
        if (our_index != -1) {
            /* Get a new entry in the library cache for the new process. */
            mutex_lock(&lib_data_mutex);
            new_index = find_cache_entry_init(lib_data, LIB_CACHE_SIZE, p);
            mutex_unlock(&lib_data_mutex);
            
            printd(true, "WAKE UP! - Copying LIB cache entry %d to %d\n", our_index, new_index);
            /* Copy the stack of return address. */
            memcpy( &lib_data[new_index].rets, &lib_data[our_index].rets, sizeof(struct stack_t) );
        }

        /* We also have to copy the 'callback cache' and 'signal cache' to
         * support the scenario where fork() is called from within a callback
         * function or signal handler: the new process must find a way to return
         * back out of the callback function.
         */
        our_index = -1;
        for (i = 0; i < CBK_CACHE_SIZE; i++) {
            if (cbk_data[i].task == current) {
                our_index = i;
            }
        }
        if (our_index != -1) {
            /* Get a new entry in the callback cache for the new process. */
            mutex_lock(&cbk_data_mutex);
            new_index = find_cache_entry_init(cbk_data, CBK_CACHE_SIZE, p);
            mutex_unlock(&cbk_data_mutex);
            
            printd(true, "WAKE UP! - Copying CBK cache entry %d to %d\n", our_index, new_index);
            /* Copy the stack of return address.  */
            memcpy( &cbk_data[new_index].rets, &cbk_data[our_index].rets, sizeof(struct stack_t) );
            
            /* Copy the stack of LBR states. We do not have to wory about the
             * task pointer within the lbr_t structs: it is not set nor used by
             * the callback cache implementation.
             */
            memcpy( &cbk_data[new_index].lbrs, &cbk_data[our_index].lbrs, sizeof(struct stack_lbr_t) );
        }

        /* Finally the 'signal cache'. Same story, but no return values to copy. */
        our_index = -1;
        for (i = 0; i < CBK_CACHE_SIZE; i++) {
            if (sig_data[i].task == current) {
                our_index = i;
            }
        }
        if (our_index != -1) {
            /* Get a new entry in the signal cache for the new process. */
            mutex_lock(&sig_data_mutex);
            new_index = find_cache_entry_init(sig_data, SIG_CACHE_SIZE, p);
            mutex_unlock(&sig_data_mutex);

            printd(true, "WAKE UP! - Copying SIG cache entry %d to %d\n", our_index, new_index);
            /* Copy the stack of LBR states. Again, no worries about the task
             * pointer within the lbr_t data structure.
             */
            memcpy( &sig_data[new_index].lbrs, &sig_data[our_index].lbrs, sizeof(struct stack_lbr_t) );
        }
                    

        /* We are almost ready to continue waking up the process. One thing left
         * is figuring out what the LBR state of the new process should look
         * like. In the case of fork() et al., we need it to mimic the state of
         * the caller task. For pthread_create(), it should be flushed. We
         * cannot tell what exactly we need though, so we will assume fork()
         * here and copy the current process' LBR state into the new one.
         * pthread_create() like functions will eventually behave like a
         * callback function, calling cbk_init() which will then flush the LBR
         * for us.
         *
         * We copy the LBR state into the cache, so that when the new task is
         * scheduled in for the fist time - and thus causing our hook to be
         * invoked - it will be copied into the actual LBR registers as if it is
         * being restored.
         */
        
//      mutex_lock(&lbr_cache_mutex);
        spin_lock_irqsave(&lbr_cache_lock, lbr_cache_flags);
        new_index = -1;
        for (i = 0; i < LBR_CACHE_SIZE; i++) {
            if (lbr_cache[i].task == NULL) {
                new_index = i;
                break;
            }
        }
        if (new_index == -1) {
            printk("Could not find empty LBR cache entry\n");
            kill_pid(task_pid(current), SIGKILL, 1);
        } else {
            printd(true, "WAKE UP! - Copying LBR cache into index %d\n", new_index);
            get_lbr(&lbr_cache[new_index]);
                     lbr_cache[new_index].task = p;
        }
        printd(true, "WAKE UP! - Unlocking spin\n");
//      mutex_unlock(&lbr_cache_mutex);
        spin_unlock_irqrestore(&lbr_cache_lock, lbr_cache_flags);
        printd(true, "WAKE UP! - Spin unlocked\n");
    }

    /* Continue to the real wake_up_new_task() implementation (in kernel/fork.c). */
    jprobe_return();

    /* UNREACHABLE */
    return 0;
}

static void do_deferred_exit(struct work_struct *work) {
    exit_work_t *exit_work = (exit_work_t *) work;
    struct task_struct *old = exit_work->task;
    int i;

    /* 3a) Remove task from library cache. */
    printd(true,"EXIT. locking lib mutex\n");
    mutex_lock(&lib_data_mutex);
    for (i = 0; i < LIB_CACHE_SIZE; i++) {
        if (lib_data[i].task == old) {
            memset(&lib_data[i], 0, sizeof(struct wrapper_lib_t));
            break;
        }
    }
    mutex_unlock(&lib_data_mutex);

    /* 3b) Remove task from callback cache. */
    printd(true,"EXIT. locking cbk mutex\n");
    mutex_lock(&cbk_data_mutex);
    for (i = 0; i < CBK_CACHE_SIZE; i++) {
        if (cbk_data[i].task == old) {
            memset(&cbk_data[i], 0, sizeof(struct wrapper_lib_t));
            break;
        }
    }
    mutex_unlock(&cbk_data_mutex);

    /* 3c) Remove task from signal cache. */
    printd(true,"EXIT. locking sig mutex\n");
    mutex_lock(&sig_data_mutex);
    for (i = 0; i < SIG_CACHE_SIZE; i++) {
        if (sig_data[i].task == old) {
            memset(&sig_data[i], 0, sizeof(struct wrapper_lib_t));
            break;
        }
    }
    mutex_unlock(&sig_data_mutex);

    printd(true, "EXIT done\n");
}

/******** task removal */
void new_do_exit(long code) {

//  struct lbr_t lbr;
    exit_work_t *exit_work;

    /* Only continue if the dying task is under our supervision. */
    if (tst_task_lbr(current)) {
        ARMOR_STAT_INC(tasks_exit);
        printd(true, "BYE BYE! - do_exit for task @ 0x%p\n",current);
/*
        get_cpu();
        get_lbr(&lbr);
        dump_lbr(&lbr);
        put_cpu();
*/
        /* There is not much that we can do here:
         * 1) We do not have to remove the LBR bit as we will never call
         *    tst_task_lbr() for this task anymore now that it is dying. 
         * 2) We do have to remove this task's scheduler hooks so that when the
         *    final sched_out() occurs, we do not store this task's LBR state
         *    infinitely.
         * 3) We do have to remove this task's items from the library, callback,
         *    and signal caches.
         */

        /* 1) Not clearing the LBR bit. */
//      clr_task_lbr(current);

        /* 2) Remove scheduler hooks. 
         *
         * We should use preemt_notifier_unregister() for this. This did not
         * work withou segfaulting the kernel though (probably since we use the
         * same preempt_notifier structure for all children that we install
         * hooks for). Instead, we now simply memset the preempt_notifiers
         * hlist_head to zero. This will remove other hooks as well, but I doubt
         * anyone cares about them since we are in do_exit() anyway. 
         */
        memset(&current->preempt_notifiers, 0, sizeof(struct hlist_head));

        exit_work = (exit_work_t*)kmalloc(sizeof(exit_work_t), GFP_ATOMIC);
        if (!exit_work) {
            BUG();
        }

        exit_work->task = current;

        INIT_WORK((struct work_struct *) exit_work, do_deferred_exit);
        queue_work(exit_queue, (struct work_struct *) exit_work);
    }

    /* Continue to the real do_exit() implementation (in kernel/exit.c). */
    jprobe_return();

    /* UNREACHABLE */
    return;
}



/******************************************************************************
 * Overloaded kernel functions to hook on dangerous system calls.
 */

/******** HOOK ON DANGEROUS SYSTEM CALL: EXECVE */
void new_sys_execve(const char __user * filename,
        const compat_uptr_t __user * argv,
        const compat_uptr_t __user * envp) {
       
    /* Only continue if this task is under our supervision. */
    if (tst_task_lbr(current)) {
        ARMOR_STAT_INC(execve);
        printd(true,"SYS_EXECVE. Starting validation process\n");
        printd(true,"current->comm: %s | filename: %s\n", current->comm, filename);
        validate_lbr();

        /* we want to disable LBR tracking this process for now. if it turns out
         * that execve failed, we will enable it again.
         * TODO: what if another thread of this process is still executing (dangerous) code while
         * execve() is being executed? Is this even possible?
         */
        printd(true,"suspending lbr tracking\n");
        susp_task_lbr(current);
    }

    /* Continue to the real sys_execve() implementation. */
}

static int new_sys_execve_ret(struct kretprobe_instance *ri, struct pt_regs *regs) {
    if (issusp_task_lbr(current)) {
        /* using ri->ret_addr does not seem to work, using pt_regs directly */
        if (regs_return_value(regs) != 0) {
            printd(true,"execve failed, waking up lbr tracking\n");
            wakeup_task_lbr(current);
        }
    }
    return 0;
}

/******** HOOK ON DANGEROUS SYSTEM CALL: MMAP */
void new_sys_mmap_pgoff(unsigned long addr, unsigned long len,
                                   unsigned long prot, unsigned long flags,
                                   unsigned long fd, unsigned long pgoff) {

    /* Only continue if this task is under our supervision. */
    if (tst_task_lbr(current)) {
        ARMOR_STAT_INC(mmap);

        /* Only validate if a region is about to be set executable. */
        if (prot & PROT_EXEC) {
            ARMOR_STAT_INC(mmap_exec);
            printd(true,"SYS_MMAP. Starting validation process\n");
            printd(true,"current->comm: %s\n", current->comm);
            validate_lbr();
        }

    }

    /* Continue to the real sys_mmap_pgoff() implementation (in mm/mmap.c). */
}

/******** HOOK ON DANGEROUS SYSTEM CALL: MPROTECT */
void new_sys_mprotect(unsigned long start, size_t len,
                                 unsigned long prot) {

    /* Only continue if this task is under our supervision. */
    if (tst_task_lbr(current)) {
        ARMOR_STAT_INC(mprotect);

        /* Only validate if a region is about to be set executable. */
        if (prot & PROT_EXEC) {
            ARMOR_STAT_INC(mprotect_exec);
            printd(true, "SYS_MPROTECT. Starting valiation process\n");
            validate_lbr();
        }
    }

    /* Continue to the real sys_mprotect() implementation (in mm/mprotect.c). */
}

/******** HOOK ON DANGEROUS SYSTEM CALL: READ */
void new_sys_read(int fd, void *buf, size_t count) {

    /* Only continue if this task is under our supervision. */
    if (tst_task_lbr(current)) {
        ARMOR_STAT_INC(read);
        validate_lbr();
    }

    /* Continue to the real sys_read() implementation. */
}

/******** HOOK ON DANGEROUS SYSTEM CALL: READ */
void new_sys_write(int fd, void *buf, size_t count) {

    /* Only continue if this task is under our supervision. */
    if (tst_task_lbr(current)) {
        ARMOR_STAT_INC(write);
        validate_lbr();
    }

    /* Continue to the real sys_write() implementation. */
}

/******** HOOK ON DANGEROUS SYSTEM CALL: SIGACTION */
void new_sys_rt_sigaction(int sig, const struct compat_sigaction __user * act,
                                                    struct compat_sigaction __user * oact,
                                        compat_size_t sigsetsize) {

    /* Only continue if this task is under our supervision. */
    if (tst_task_lbr(current)) {
        ARMOR_STAT_INC(sigaction);
        printd(true, "SYS_RT_SIGACTION. Starting valiation process\n");
        validate_lbr();
    }

    /* Continue to the real sys_rt_sigaction() implementation (in kernel/signal.c). */
}


int armor_open(struct inode *inode, struct file *filp) {
    printd(false, "armor-module is being opened\n");

    armor_opens++;
    if (armor_initialized) return 0;

    /* Initialize a bunch of data structures. */
    mutex_init(&lib_data_mutex);
    mutex_init(&cbk_data_mutex);
    mutex_init(&sig_data_mutex);

    spin_lock_init(&lbr_cache_lock);

    armor_desc.tfm = crypto_alloc_hash(DIGEST_TYPE, 0, CRYPTO_ALG_ASYNC);
    if (IS_ERR(armor_desc.tfm)) {
        printk("Could not allocate crypto hash\n");
        return -1;
    }

#ifdef ARMOR_JIT
    mutex_init(&validation_lock);
    sema_init(&jit_work_cond,   0);
    sema_init(&jit_result_cond, 0);

    jit_waittime = msecs_to_jiffies(25000);
#endif

    sema_init(&target_started, 0);
    sema_init(&plt_available, 0);
    sema_init(&exitpoints_available, 0);

    main_task = NULL;

    armor_initialized = 1;
    return 0;
}

int armor_close(struct inode *inode, struct file *filp) {
    armor_opens--;

    if (armor_opens) return 0;

    /* Free some data strucutures/ */

//  if (!IS_ERR(armor_desc.tfm)) crypto_free_hash(armor_desc.tfm);
    if (paths != NULL) {
        kfree(paths);
        paths = NULL;
    }

#ifdef ARMOR_STATS
    printk("Some statistics:\n");
    printk("Signal handlers entered: %llu\n", stats.sig_enter);
    printk("Signal returns:          %llu\n", stats.sig_return);
    printk("Library calls:    %llu\n", stats.lib_enter);
    printk("Library returns:  %llu\n", stats.lib_return);
    printk("Callback calls:   %llu\n", stats.cbk_enter);
    printk("Callback returns: %llu\n", stats.cbk_return);
    printk("\n");
    printk("Number of newly created tasks: %llu\n", stats.new_tasks);
    printk("Number of exited tasks:        %llu\n", stats.tasks_exit);
    printk("\n");
    printk("System calls to execve:    %llu\n", stats.execve);
    printk("System calls to read:    %llu\n", stats.read);
    printk("System calls to write:    %llu\n", stats.write);
    printk("System calls to mmap:      %-5llu (only PROT_EXEC: %llu)\n", stats.mmap, stats.mmap_exec);
    printk("System calls to mprotect:  %-5llu (only PROT_EXEC: %llu)\n", stats.mprotect, stats.mprotect_exec);
    printk("System calls to sigaction: %llu\n", stats.sigaction);
    printk("System calls total:        %llu\n",stats.execve + stats.read + stats.write + stats.mmap + stats.mprotect + stats.sigaction);
    printk("\n");
    printk("Context switches (sched-out): %llu\n", stats.sched_out);
    printk("Context switches (sched-in):  %llu\n", stats.sched_in);
    printk("\n");
    printk("Number of digests computed: %llu\n", stats.digests);
    printk("LBR JIT lookups:     %llu\n", stats.jit_lookups);
    printk("LBR JIT cache hits:  %llu\n", stats.jit_cache_hits);
    printk("LBR JIT hits:        %llu\n", stats.jit_hits);
    printk("LBR JIT misses:      %llu\n", stats.jit_misses);
    printk("LBR JIT unsupported: %llu\n", stats.jit_unsupported);
    printk("LBR JIT timeouts:    %llu\n", stats.jit_timeouts);
    printk("LBR JIT sec:         %llu\n", stats.jit_sec);
    printk("LBR JIT usec:        %llu\n", stats.jit_usec);
#endif

    printd(false, "armor-module is now closed\n");

    armor_initialized = 0;

    return 0;
}

int armor_ioctl_get_offsets(unsigned long arg1)
{
    struct offsets_t *offsets;
    
    struct mm_struct *mm;
    struct vm_area_struct *mmap;
    struct file *file;
    struct path path;
    char *pathname;
    char *tmp;

    int i;
    
    printk("Waiting for target to start...\n");
    if (down_interruptible(&target_started) != 0) {
        printd(true,"GET_OFFSETS: INTERRUPTED!\n");
        return -ERESTARTSYS;
    }


    printk("Getting offsets\n");
    offsets = kmalloc(sizeof(struct offsets_t), GFP_KERNEL);
    if (offsets == NULL) {
        printk("Could not allocate %lu bytes of kernel memory\n", sizeof(struct offsets_t));
        return -ENOMEM;
    }


    offsets->indirect_call_source  = tocopy.indirect_call_source;
    offsets->load_from             = tocopy.load_from;
    offsets->armor_lib_enter       = tocopy.armor_lib_enter;
    offsets->armor_lib_return      = tocopy.armor_lib_return;
    offsets->pthread_create        = tocopy.pthread_create;
    offsets->pthread_create_return = tocopy.pthread_create_return;

    if(!main_task) {
        printk("armor: no main task yet!\n");
        return -EINVAL;
    }

    mm  = main_task->active_mm;
    if(!mm) {
        printk("armor: main task has no active_mm!\n");
        return -EINVAL;
    }
    mmap = mm->mmap;
    i = 0;
    do {
	if(!mmap) {
		printk("armor: no mmap!\n");
		return -EINVAL;
        }
        if (mmap->vm_flags & VM_EXEC) {
            offsets->start[i] = mmap->vm_start;
            offsets->end[i]   = mmap->vm_end;
            if (mmap->vm_file != NULL) {

                file = mmap->vm_file;

		if(!file) {
			printk("armor: no mmap->vm_file!\n");
			return -EINVAL;
		}
                
                spin_lock(&file->f_lock);

                path = file->f_path;
                path_get(&file->f_path);

                spin_unlock(&file->f_lock);

                tmp = (char *)__get_free_page(GFP_TEMPORARY);

                if (!tmp) {
                    path_put(&path);
                    printk("Could not get free page\n");
                    return -ENOMEM;
                }

                pathname = d_path(&path, tmp, PAGE_SIZE);
                path_put(&path);

                if (IS_ERR(pathname)) {
                    printk("Could not d_path()\n");
                    free_page((unsigned long)tmp);
                    return PTR_ERR(pathname);
                }

                strncpy(offsets->name[i], pathname, MAX_LIBNAME_LENGTH-1);

                free_page((unsigned long)tmp);
            } else {
                strcpy(offsets->name[i], "<unknown>");
            }
            i++;
        }
        mmap = mmap->vm_next;
    } while (mmap != NULL);
    offsets->libs = i;


    printk("Copying to user\n");
    if (copy_to_user((void __user *)arg1, offsets, sizeof(struct offsets_t))) {
        printk("Could not copy offsets to user\n");
        return 1;
    }
    kfree(offsets);

    return 0;
}




/******************************************************************************
 * Handler for IOCTLs. This is the actual kernel module interface.
 */
static long armor_ioctl(struct file *file, unsigned int cmd, unsigned long arg1) {
    uint64_t code[2] , *retaddr_p;
    uint64_t *plts;
    uint64_t *gots;
    uint64_t *exits;
    int i;
    int ret;

    /* Using an if-statement instead of switch case for the sensitive functions
     * may speed up things a bit.
     */

/******** LIB ENTER */
    if (cmd == ARMOR_IOC_LIB_ENTER) {
        /* Entering a library function. */
        ARMOR_STAT_INC(lib_enter);
        printd(true, "LIB_ENTER - storing   return address: %lx\n", arg1);
        /* This adds the current task to the 'lib cache' array and stores
         * the return address (arg1) on a stack data structure. 
         */
        if (arg1) lib_init(arg1);
        else {
            /* No return address to store. This happens for functions
             * wrapped 'manually' using LD_PRELOAD (like pthread_create). We
             * only have to disable the LBR.
             */
        }
        wrmsrl(MSR_IA32_DEBUGCTLMSR, 0);
        return 0;
    }

/******** LIB EXIT */
    if (cmd == ARMOR_IOC_LIB_EXIT) {
        /* Returning from a library function. */
        ARMOR_STAT_INC(lib_return);
        /* Search the current task in the 'lib cache' array so that we can
         * return its return address and remove it from the cache to make
         * room for other processes.
         */
        if (arg1) {
             retaddr_p = (uint64_t*) arg1;
            *retaddr_p = lib_exit();
            printd(true, "LIB_EXIT  - restoring return address: %llx\n", *retaddr_p);
        } else {
            printd(true, "LIB_EXIT  - restoring return address: %llx\n", (uint64_t)0x00);
            /* No return address to restore. This happens for functions
             * wrapped 'manually' using LD_PRELOAD (like pthread_create). We
             * only have to enable the LBR again.
             */
        }
        wrmsrl(MSR_IA32_DEBUGCTLMSR, IA32_DEBUGCTL);
        return 0;
    }


    switch (cmd) {

/******** COPY PATHS */
        case ARMOR_IOC_COPY_PATHS:
            printd(true,"ARMOR_COPY\n");

            if (copy_from_user(&tocopy, (void __user *)arg1, sizeof(struct lbr_paths_kernel_copy)) != 0) {
                printk("Could not copy copy-struct to kernel\n");
                return 1;
            }
            
            paths = kmalloc(tocopy.size, GFP_KERNEL);
            if (paths == NULL) {
                printk("Could not allocate %lu bytes of kernel memory\n", tocopy.size);
                return 1;
            }
            if (tocopy.paths != NULL) {
                if (copy_from_user(paths,  (void __user *)tocopy.paths, tocopy.size) != 0) {
                    printk("could not copy\n");
                    return 1;
                }
            
                lbr_link_paths(paths);
                for (i = 0; i < paths->funcs; i++) {
                    printd(true,"- Path for %s @ %llx\n", paths->func[i].fname, paths->func[i].fptr);
                }
#ifdef ARMOR_DEBUG
                dump_paths(paths);
#endif

            }

            main_task = current;
            /* it seems we have enough information to hook up the JIT analyzer,
             * let's wake it up. */
            /* TODO: there is a potional error here. When unlocking the JIT analyzer here,
             * it will read the mmap data for main_task. However, if the main program does a quick
             * fork and then exits the parents, this struct will be gone. We should solve this by
             * doing a nother lock here (wait until the JIT analyzer is finished
             * reading mmap data). However, it is probably safe to assume that
             * this will never happen: The target program will be busy getting
             * instrumented before it has time to fork. Meanwhile, the JIT
             * analyzer should get all the data it want and we're good...
             */
            up(&target_started);

            return 0;

/******** INIT */
        case ARMOR_IOC_INIT:
            printd(true, "ARMOR_INIT\n");
            /* This should be called from user space right before we start
             * executing our target.       
             */

            if (arg1 == 42) {
                /* process doing exec. */
                printk("!!! OUR WRAPPER IS BEING CALLED FOR A SECOND TIME !!!\n");
                printk("!!!            WE DO NOT SUPPORT EXEC             !!!\n");
//              kill_pid(task_pid(current), SIGKILL, 1);

                /* we would actualy want to start instrumentation for the new process. */


                return 0;
            }

    memset(lib_data, 0, sizeof(lib_data));
    memset(cbk_data, 0, sizeof(cbk_data));
    memset(sig_data, 0, sizeof(sig_data));
                
    /* Initialize the LBR cache. */
    init_lbr_cache();
    
#ifdef ARMOR_STATS
    memset(&stats, 0, sizeof(stats));
#endif
#ifdef ARMOR_JIT
    memset(&jit_cache, 0, sizeof(jit_cache));
    disable_jit = 0;
#endif
    preempt_notifier_register(&notifier);

            /* Enable the LBR on each CPU. */
            on_each_cpu(enable_lbr, NULL, 1);
            
            /* Set the 'I am being LBRed'-bit. */
            set_task_lbr(current);

            /* Return the address space region of the target process. */
            code[0] = current->mm->start_code;
            code[1] = current->mm->end_code;
            printd(true, "ARMOR_INIT, start: %llx\n",code[0]);
            printd(true, "ARMOR_INIT, end  : %llx\n",code[1]);
            ret = copy_to_user((void __user *)arg1, code, sizeof(code));
            printd(true,"copied!\n");
            return ret;

/******** HELPER IOCTLS */
        case ARMOR_IOC_NOTHING:
            printd(true, "------------------------------------\n");
            return 0;

        case ARMOR_IOC_VALIDATE:
            validate_lbr();
            return 0;

        case ARMOR_IOC_DISABLE_LBR:
            wrmsrl(MSR_IA32_DEBUGCTLMSR, 0);
            return 0;

        case ARMOR_IOC_ENABLE_LBR:
            wrmsrl(MSR_IA32_DEBUGCTLMSR, IA32_DEBUGCTL);
            return 0;

        case ARMOR_IOC_DUMP:
            /* not implemented. */
            return 0;

        

/******** CALLBACK ENTER */
        case ARMOR_IOC_CALLBACK_ENTER:
            /* Entering a callback function. */
            ARMOR_STAT_INC(cbk_enter);

            printd(true,"CBK_ENTER. Starting validation process\n");
            validate_lbr();

            printd(true, "CBK_ENTER - storing   return address: %lx\n", arg1);
            /* This adds the current task to the 'callback cache' array and
             * stores both the return address (arg1) and the current LBR state.
             * It also flushes and (re)enables the LBR.
             */
            cbk_init(arg1);
            return 0;

/******** CALLBACK EXIT */
        case ARMOR_IOC_CALLBACK_EXIT:
            /* Returning from a callback function. */
            ARMOR_STAT_INC(cbk_return);

            /* Search the current task in the 'callback cache' array so that we
             * can return its return address and restore the LBR state.
             */
            if (arg1) {  
                 retaddr_p = (uint64_t*) arg1;
                *retaddr_p = cbk_exit();
                printd(true, "CBK_EXIT  - restoring return address: %llx\n", *retaddr_p);
            } else {
                printd(true, "CBK_EXIT  - restoring return address: %llx\n", (uint64_t)0x00);
                /* No return address to restore. This happens for functions
                 * wrapped 'manually' using LD_PRELOAD (like pthread_create). We
                 * do have to call cbk_exit() though, to ensure we restore the
                 * LBR state.
                 */
                cbk_exit();
            }
            return 0;

#ifdef ARMOR_JIT
        case ARMOR_IOC_GET_JIT_WORK:
            /* Return JIT work if there is any. */
            printd(true,"GET_JIT - JIT analyzer is waiting for work now\n");

            /* Wait for work. */
            if (down_interruptible(&jit_work_cond) != 0) {
                printd(true,"GET_JIT: INTERRUPTED!\n");
                return -EINTR;
            }
  
            /* Copy the LBR state to user space. */
            printd(true,"GET_JIT - Copying LBR to user space\n");
            return copy_to_user((void __user *)arg1, jit_work, sizeof(struct lbr_t));
        
        case ARMOR_IOC_PUT_JIT_WORK:
            /* Receive a reply from the analyzer. */
            printd(true,"PUT_JIT - JIT analyzer returned %d\n", arg1);

            /* Store the validation reply. */
            jit_result = arg1;
            
            printd(true, "Notifying validator\n");
            /* Notify the validator that the result is ready. */
            up(&jit_result_cond);

            return 0;
#endif 
        case ARMOR_IOC_GET_OFFSETS:
            return armor_ioctl_get_offsets(arg1);

        case ARMOR_IOC_PUSH_PLTS:
            if (copy_from_user(&plt_got_copy, (void __user *)arg1, sizeof(struct plt_got_copy_t)) != 0) {
                printk("Could not copy plt-got-struct to kernel\n");
                return 1;
            }
            
            plts = kmalloc(plt_got_copy.size, GFP_KERNEL);
            if (plts == NULL) {
                printk("Could not allocate %lu bytes of kernel memory\n", plt_got_copy.size);
                return 1;
            }
            if (copy_from_user(plts,  (void __user *)plt_got_copy.plts, plt_got_copy.size) != 0) {
                printk("could not copy\n");
                return 1;
            }
            
            gots = kmalloc(plt_got_copy.size, GFP_KERNEL);
            if (gots == NULL) {
                printk("Could not allocate %lu bytes of kernel memory\n", plt_got_copy.size);
                return 1;
            }
            if (copy_from_user(gots,  (void __user *)plt_got_copy.gots, plt_got_copy.size) != 0) {
                printk("could not copy\n");
                return 1;
            }

            printk("Got a PLT - GOT table:\n");
            for (i = 0; i < plt_got_copy.items; i++) {
                printk("%p: %p\n", (void *) plts[i], (void *) gots[i]);
            }

            plt_got_copy.plts = plts;
            plt_got_copy.gots = gots;

            up(&plt_available);

            return 0;
        case ARMOR_IOC_PUSH_EXITS:
            if (copy_from_user(&simples, (void __user *)arg1, sizeof(struct simples_t)) != 0) {
                printk("Could not copy simples struct to kernel\n");
                return 1;
            }

            exits = kmalloc(simples.size, GFP_KERNEL);
            if (exits == NULL) {
                printk("Could not allocate %lu bytes of kernel memory\n", simples.size);
                return 1;
            }
            if (copy_from_user(exits, (void __user *)simples.exitpoints, simples.size) != 0) {
                printk("could not copy\n");
                return 1;
            }

            printk("Got a exit points table\n");
            for (i = 0; i < simples.items; i++) {
                printk("%p\n", (void *) exits[i]);
            }
            simples.exitpoints = exits;

            up(&exitpoints_available);

            return 0;
            

        case ARMOR_IOC_PULL_PLT_COPY:
            printk("Waiting for library instrumentation to finish...\n");
            if (down_interruptible(&plt_available) != 0) {
                printd(true,"PULL_PLT_COPY INTERRUPTED!\n");
                return -ERESTARTSYS;
            }
            printk("Copying PLT-GOT meta data\n");
            return copy_to_user((void __user *)arg1, &plt_got_copy, sizeof(struct plt_got_copy_t));

        case ARMOR_IOC_PULL_PLTS:
            printk("Copying PLT data\n");
            return copy_to_user((void __user *)arg1, plt_got_copy.plts, plt_got_copy.size);
        case ARMOR_IOC_PULL_GOTS:
            printk("Copying GOT data\n");
            return copy_to_user((void __user *)arg1, plt_got_copy.gots, plt_got_copy.size);

        case ARMOR_IOC_PULL_EXITS:
            printk("Waiting for exit points to become ready...\n");
            if (down_interruptible(&exitpoints_available) != 0) {
                printd(true,"PULL_EXITS INTERRUPTED!\n");
                return -ERESTARTSYS;
            }
            printk("Copying simples\n");
            return copy_to_user((void __user *)arg1, &simples, sizeof(struct simples_t));

        case ARMOR_IOC_PULL_EXITS_DATA:
            printk("Copying exitpoints\n");
            return copy_to_user((void __user *)arg1, simples.exitpoints, simples.size);



        default:
            return -ENOTTY;
    }
    return 0;
}




/******************************************************************************
 * Module setup.
 */
static struct file_operations armor_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = armor_ioctl,
    .open           = armor_open,
    .release        = armor_close,
};
static struct miscdevice armor_miscdev = {
    .minor          = MISC_DYNAMIC_MINOR,
    .name           = "armor",
    .fops           = &armor_fops,
};

static struct jprobe probe_wake_up_new_task = { .entry = (kprobe_opcode_t *) new_wake_up_new_task };
static struct jprobe probe_do_exit          = { .entry = (kprobe_opcode_t *) new_do_exit          };
static struct kretprobe probe_execve        = { .handler = new_sys_execve_ret, .maxactive = NR_CPUS };


static int __init armor_init(void) {

    /* Init hooks on context switches. */
    preempt_notifier_init(&notifier, &ops);

#ifdef ARMOR_STATS
    sysctl_header = register_sysctl_table(armor_dir);
    if (sysctl_header == NULL) {
        printk(KERN_ERR "cannot register sysctl table\n");
        return -1;
    }
#endif
    
    exit_queue = create_workqueue("exit_queue");
    if (!exit_queue) {
        printk(KERN_ERR "cannot create work queue\n");
        goto err_unroll_register_sysctl;
    }

    probe_wake_up_new_task.kp.addr = (kprobe_opcode_t *) kallsyms_lookup_name("wake_up_new_task");
    if (!probe_wake_up_new_task.kp.addr) {
        printk(KERN_ERR "cannot lookup wake_up_new_task\n");
        goto err_unroll_workqueue;
    }

    probe_do_exit.kp.addr          = (kprobe_opcode_t *) kallsyms_lookup_name("do_exit");
    if (!probe_do_exit.kp.addr) {
        printk(KERN_ERR "cannot lookup do_exit\n");
        goto err_unroll_lookup_new_task;
    }

    probe_execve.kp.addr = (kprobe_opcode_t *) kallsyms_lookup_name("sys_execve");
    if (!probe_execve.kp.addr) {
        printk(KERN_ERR "cannot lookup sys_execve\n");
        goto err_unroll_lookup_new_task;
    }

    if (register_jprobe(&probe_wake_up_new_task)) {
        printk(KERN_ERR "cannot register wake_up_new_task\n");
        goto err_unroll_lookup_do_exit;
    }

    if (register_jprobe(&probe_do_exit)) {
        printk(KERN_ERR "cannot register do_exit\n");
        goto err_unroll_register_new_task;
    }

    if (register_kretprobe(&probe_execve)) {
        printk(KERN_ERR "cannot register execve\n");
        goto err_unroll_register_do_exit;
    }

    if (intercept_syscalls_init() < 0) {
        printk(KERN_ERR "cannot intercept system calls\n");
        goto err_unroll_register_execve;
    }

    if (intercept_symbol("signal_delivered", &pre_signal_delivered, pre_signal_delivered_exit) < 0) {
        printk(KERN_ERR "cannot intercept signal_delivered\n");
        goto err_unroll_intercept_syscalls;
    }
  
    if (misc_register(&armor_miscdev)) {
        printk(KERN_ERR "cannot register miscdev on minor=%d\n", ARMOR_MINOR);
        goto err_unroll_intercept_signal_delivered;
    }
    
    printk("Armor module initialized\n");
    return 0;

err_unroll_intercept_signal_delivered:
    restore_symbol("signal_delivered");
err_unroll_intercept_syscalls:
    intercept_syscalls_exit();
err_unroll_register_execve:
    unregister_kretprobe(&probe_execve);
err_unroll_register_do_exit:
    unregister_jprobe(&probe_do_exit);
err_unroll_register_new_task:
    unregister_jprobe(&probe_wake_up_new_task);
err_unroll_lookup_do_exit:
err_unroll_lookup_new_task:
err_unroll_workqueue:
    flush_workqueue(exit_queue); 
    destroy_workqueue(exit_queue);
err_unroll_register_sysctl:
#ifdef ARMOR_STATS
    unregister_sysctl_table(sysctl_header);
#endif
    return -1;
}

static void __exit armor_exit(void) {
    restore_symbol("signal_delivered");
    intercept_syscalls_exit();
    unregister_kretprobe(&probe_execve);
    unregister_jprobe(&probe_do_exit);
    unregister_jprobe(&probe_wake_up_new_task);
    flush_workqueue(exit_queue); 
    destroy_workqueue(exit_queue);
#ifdef ARMOR_STATS
    unregister_sysctl_table(sysctl_header);
#endif
    misc_deregister(&armor_miscdev);
}

module_init(armor_init);
module_exit(armor_exit);

