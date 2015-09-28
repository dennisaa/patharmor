//#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <link.h>

#include "armor.h"
#include "lbr-state.h"

#include "../../include/common/util/safeio.h"

/* Function pointer to the target's main() function */
static int (*orig_main) (int, char **, char **);

/* Global file descriptor for /dev/armor */
int armor_fd = 0;

uint64_t armor_start_code = 0x000000000000;
uint64_t armor_end_code   = 0x7f0000000000;
uint64_t armor_range_code = 0x7f0000000000;
    
int (*real_pthread_create)(pthread_t *thread, const pthread_attr_t *attr, 
                            void *(*start_routine) (void *), void *arg);


/* Inline IOCTL syscall to our armor module */
#define ARMOR_INLINE_IOCTL( _ret, _ioc, _arg ) { asm (  \
            "syscall;"        \
            : "=a"(_ret)      \
            : "a"(SYS_ioctl), \
              "D"(armor_fd),  \
              "S"(_ioc),      \
              "d"(_arg)       \
            : "%rcx", "%r11"  \
        ); }
/* In 'plain' C and with comments:
 * asm ( "syscall;" 
 *       : "=a"(ret) 
 *       : "a"(SYS_ioctl),           // rax = syscall number. 16 for ioctl on x64
 *         "D"(fd),                  // rdi = 1st argument: fd
 *         "S"(ARMOR_IOC_INIT),      // rsi = 2nd argument: ARMOR_IOC_INIT
 *         "d"(0)                    // rdx = 3rd argument: 0
 *       : "%rcx", "%r11"            // kernel may destroy these
 *     );
 * 
 * More on GCC extended inline assembly:
 * - http://www.ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html
 *
 * The following is an ever faster variant that accepts zero arguments and
 * discards the syscall's return value.
 */
#define ARMOR_INLINE_IOCTL_FAST( _ioc ) { asm (  \
                    "syscall;"        \
                    :                 \
                    : "a"(16),        \
                      "D"(armor_fd),  \
                      "S"(_ioc),      \
                      "d"(0)          \
                    : "%rcx", "%r11"  \
                ); }






extern void *armor_lib_enter();
extern void *armor_lib_return;
extern void *armor_cbk_enter();
extern void *armor_cbk_return;
extern void *armor_cbk_target_call(); // will be in the LBR
extern void *armor_cbk_target_return; // also

extern void *armor_pthread_enter();

// forward declarations
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, 
                   void *(*start_routine) (void *), void *arg);
void pthread_create_marker(void);


/* This function initalizes the armor module:
 * - open the /dev/armor special device so we can communicate with the kernel
 *
 * The caller is responsible for making a final ARMOR_IOC_INIT ioctl to complete
 * the armor kernel module's initialization. This must be done inline to avoid a
 * return statement from libc's ioctl() implementation that pollutes the LBR. 
 * The caller is going to need the file descriptor during execution of the
 * target program, so that library calls can be instrumented and perform a
 * syscall to our module. This is why armor_fd is currently global.
 */
void initialize_armor(uint64_t load_from) {
    int ret;

    if (load_from == 0) goto open_armor;

    fprintf(stderr,"main() - LBR.src: 0x%lx\n",load_from);
    fprintf(stderr,"main() - LBR.dst: %p\n",orig_main);
    fprintf(stderr,"armor_lib_enter        : %p\n", armor_lib_enter);
    fprintf(stderr,"armor_lib_exit.return(): %p\n", &armor_lib_return);
    fprintf(stderr,"armor_cbk_enter        : %p\n", armor_cbk_enter);
    fprintf(stderr,"armor_cbk_return       : %p\n", &armor_cbk_return);
    fprintf(stderr,"armor_cbk_target_call  : %p\n", armor_cbk_target_call);
    fprintf(stderr,"armor_cbk_target_return: %p\n", &armor_cbk_target_return);
    fprintf(stderr,"pthread_create         : %p\n", pthread_create);
    fprintf(stderr,"pthread_create_return  : %p\n", pthread_create_marker -1);
            
open_armor:
    /* Open the armor special device */
    armor_fd = util_safeio_get_fd(open("/dev/armor", O_RDONLY));
    if (armor_fd < 0) {
        perror("[wrapper]: could not open /dev/armor");
        exit(EXIT_FAILURE);
    }

    if (load_from == 0) {
        ARMOR_INLINE_IOCTL (ret, ARMOR_IOC_INIT, 42);
        return;
    }
    
    struct lbr_paths_kernel_copy tocopy;
    tocopy.paths = NULL;
    tocopy.size  = 0;

    tocopy.indirect_call_source  = (uint64_t) armor_cbk_target_call;
    tocopy.load_from             = (uint64_t) load_from;
    tocopy.armor_lib_enter       = (uint64_t) &armor_lib_enter;
    tocopy.armor_lib_return      = (uint64_t) &armor_lib_return;
    tocopy.pthread_create        = (uint64_t) pthread_create;
    tocopy.pthread_create_return = (uint64_t) pthread_create_marker - 1;
  
    /* Copy the valid states to the armor module */
    ret = ioctl(armor_fd, ARMOR_IOC_COPY_PATHS, &tocopy);
    if (ret) {
        perror("[wrapper]: could not initialize armor module");
        exit(EXIT_FAILURE);
    }

    /* Returning now, caller should setup an inline syscall to perform
     * the final ARMOR_IOC_INIT ioctl.      
     */

}










/* This is the overriden main function that is called by __libc_start_main(). It
 * will initialize the armor module and call the original target's main()
 * function.
 */
static int armor_main(int argc, char **argv, char **envp) {
    register int rax asm("rax");
    uint64_t code[2]; // to get the start_code and end_code from the kernel
    int ret;


    // Only continue if this is the actual target process
    printf("[wrapper]: this is %s\n", argv[0]);
    if (strstr(argv[0], "di-opt") != NULL) return orig_main(argc, argv, envp);

    if (getenv("PA_INITIALIZED")) {
        initialize_armor(0);
        return orig_main(argc, argv, envp);
    }






    /* Compute the address that will occupy the 'first' LBR.from entry. This
     * address can be found near calllabel and makes a call to the target's main
     * function.
     * See below for more details.
     */
    void *load_from = &&calllabel - 2; 
    
    /* Initialize the armor module */
    initialize_armor((uint64_t)load_from);


    /* The following code performs the final ARMOR_IOC_INIT ioctl system call.
     * By using inline assembler, we avoid a LBR polluting return statement from
     * the libc ioctl() implementation.
     * 
     * The ARMOR_IOC_INIT ioctl is responsible for enabling LBR tracing on all
     * CPUs and flushing its entries. The only indirect branch that should
     * follow is a call to the original main function.
     * 
     * ret = ioctl(armor_fd, ARMOR_IOC_INIT, code);
     */
    setenv("PA_INITIALIZED", "1", 1);
    fprintf(stderr,"[wrapper]: ARMOR_IOC_INIT\n");
    ARMOR_INLINE_IOCTL ( ret, ARMOR_IOC_INIT, code );
    if (ret) {
        perror("[wrapper]: could not initialize armor module");
        exit(EXIT_FAILURE);
    }

    armor_start_code = code[0];
//  armor_end_code   = code[1];
    armor_end_code   = 0x7f0000000000;
    armor_range_code = armor_end_code - armor_start_code;
    // we cannot print here, it would pollute the LBR
//  libc_printf("start_code: %p\n", armor_start_code);
//  libc_printf("  end_code: %p\n", armor_end_code);
//  libc_printf("range_code: %p\n", armor_range_code);

    /* Execute the target's main function */
    orig_main(argc, argv, envp);
    /* Which, in assembly, would look like:
     *      ff d0                   callq  *%rax
     *      ?? ??                   <next instruction>
     * By adding a label right after calling orig_main, we can get the address
     * of <next instruction> instruction (via &&label). Assuming that invoking
     * main is always done by a callq *%rax and thus consumes only 2 bytes, we 
     * can now compute the address of the call instruction. We use this
     * information to update the set of valid paths: the address computed here
     * will be the first LBR.from entry.
     */
calllabel:

    /* We now fetch the return address by reading it from %rax. If we would have
     * used ret = orig_main(...) instead, then another mov instructions would be
     * in place before our calllabel:
     *      89 45 f4                mov    %eax,-0xc(%rbp)
     * Relative addressing makes it hard to estimate the exact number of bytes
     * such mov instruction would occupy, which is why this 'hack' is used.
     */
    ret = rax;

    /* We don't close the file descriptor here but rather wait for it to be
     * closed automatically when the last process dies.
     */

    return ret;
}




/* For normal, GCC compiled programs, main() is called by __libc_start_main(), a
 * function that initializes libc so that it can be used by the program. In our
 * approach, we override the program's __libc_start_main() function. Our
 * __libc_start_main() function will simply call the original __libc_start_main
 * with the only difference that instead of a pointer to the target's main() a
 * function pointer to our armor_main() function is provided. In armor_main(),
 * we then do all our Armor and LBR initalization before making a final call to
 * the original target's main().
 *
 * The implementation to override __libc_start_main and the target's main() is
 * based on:
 * https://chromium.googlesource.com/chromiumos/platform/minijail/+/toolchainA/libminijailpreload.c
 */

int __libc_start_main(int (*main) (int, char **, char **), int argc, char **ubp_av, void (*init) (void),  void (*fini) (void), void (*rtld_fini) (void), void (*stack_end)) {
    /* Store the original main() function pointer so that we can call it in
     * armor_main after initialization is done. */
    orig_main = main;

    /* function pointer to libc's __libc_start_main() */
    typeof(__libc_start_main) *real_libc_start_main;

    /* Find the next occurence of __libc_start_main() */
    real_libc_start_main = dlsym(RTLD_NEXT, "__libc_start_main");

    /* Continue with the real __libc_start_main(), replacing the main() function
     * with our armor_main().
     */
    return (*real_libc_start_main)(armor_main, argc, ubp_av, init, fini, rtld_fini, stack_end);

}


struct armor_pthread_data {
    void *(*start_routine) (void *);
    void *arg;
};

// special case for pthread_create
void *armor_pthread(void *arg) {
    struct armor_pthread_data *p = (struct armor_pthread_data *)arg;

    void *thread_function = p->start_routine;
    void *thread_args     = p->arg;
    free(p);

    /* We want to make the indirect call from armor_cbk_enter so that we always
     * get the same src address for jumps to AT functions. We use an assembly
     * stub for that. */
    return armor_pthread_enter(thread_args, thread_function);
}


int pthread_create(pthread_t *thread, const pthread_attr_t *attr, 
                   void *(*start_routine) (void *), void *arg) {

    int ret;

    ARMOR_INLINE_IOCTL_FAST(ARMOR_IOC_LIB_ENTER);

    /* Get the original address of pthread_create as we will wrap that function
     * manually using LD_PRELOAD
     */
    if (real_pthread_create == NULL) real_pthread_create = dlsym(RTLD_NEXT, "pthread_create");

    if (armor_fd != 0 &&
        (uint64_t) start_routine > armor_start_code && 
        (uint64_t) start_routine < armor_end_code) {

        struct armor_pthread_data *p = malloc(sizeof(struct armor_pthread_data));
        if (p == NULL) {
            perror("malloc failed\n");
            exit(EXIT_FAILURE);
        }
        p->start_routine = start_routine;
        p->arg = arg;

        ret = (*real_pthread_create)(thread, attr, armor_pthread, p);
    } else { 
        ret = (*real_pthread_create)(thread, attr, start_routine, arg);
    }

    ARMOR_INLINE_IOCTL_FAST(ARMOR_IOC_LIB_EXIT);

    return ret;
}
void pthread_create_marker(void) { 
}
