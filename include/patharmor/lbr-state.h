#ifndef LBR_STATE_H
#define LBR_STATE_H

#ifndef __KERNEL__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif 

#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <stdint.h>

#endif /* __KERNEL__ */


/* Which digest to use. MD4 would be the fasted. A different scheme in which we
 * use a simple XOR key or a checksum would work as well: good luck finding a
 * collision with valid addresses to gadgets. */
#define DIGEST_SHA1 "sha1"
#define DIGEST_MD5  "md5"
#define DIGEST_MD4  "md4"

#define DIGEST_TYPE DIGEST_MD4

/* SHA1: 20 bytes
 * MD4:  16 bytes
 * MD5:  16 bytes 
 */
#define DIGEST_LENGTH 16

/* The following ctx size is only used when using the _shash_ crypto functions.
 * See comments in lbr.c for more info.
 * SHA1: 96 bytes
 * MD4:  88 bytes
 * MD5:  88 bytes
 */
#define DIGEST_CTX_SIZE 88


/* This one is also defined in lbr.h, but lbr.h is not included (as it is only
 * kernel stuff there). 
 */
#define LBR_ENTRIES 16


#define MAX_FNAME_LENGTH 16     // max length of the library/system call function name

/* This struct holds basic information for functions that require LBR validating
 * (so-called 'blacklisted' functions). 
 */
struct lbr_function {
    uint64_t fptr;
    char     fname[MAX_FNAME_LENGTH];
};

struct lbr_t {
    uint64_t debug;   // contents of IA32_DEBUGCTL MSR
    uint64_t select;  // contents of LBR_SELECT
    uint64_t tos;     // index to most recent branch entry
    uint64_t from[LBR_ENTRIES];
    uint64_t   to[LBR_ENTRIES];
    struct task_struct *task; // pointer to the task_struct this state belongs to
};

/* Although we plan to validate the complete path (all 16 LBR entries), it may
 * be impossible to generate such amount of CFI data within a reasonable time
 * frame during static analysis. For this, we support different window sizes
 * where we validate the LBR on shorter intervals. When the window size is set
 * to two, for example, static analysis has to compute paths to address x of
 * length two only.
 * The following table illustrates the number of hashes that the kernel module
 * shall compute for a number of different window sizes.
 *
 * LBR  WINDOW_SIZE 2       WIN 3     WIN 4   WIN 5 WIN 6 WIN 7 WIN 8 WIN 9 WIN 16
 * 0    a                   a         a       a     a     a     a     a     a       
 * 1    a b                 a         a       a     a     a     a     a     a
 * 2      b c               a b       a       a     a     a     a     a     a
 * 3        c d               b       a b     a     a     a     a     a     a
 * 4          d e             b c       b     a b   a     a     a     a     a
 * 5            e f             c       b       b   a b   a     a     a     a
 * 6              f g           c d     b c     b     b   a b   a     a     a
 * 7                g h           d       c     b     b     b   a b   a     a
 * 8                  h i         d e     c     b c   b     b     b   a b   a
 * 9                    i j         e     c d     c   b     b     b     b   a
 * 10                     j k       e f     d     c   b c   b     b     b   a
 * 11                       k l       f     d     c     c   b     b     b   a
 * 12                         l m     f g   d e   c d   c   b c   b     b   a
 * 13                           m n     g     e     d   c     c   b     b   a
 * 14                             n o   g h   e     d   c     c   b c   b   a
 * 15                               o     h   e     d   c     c     c   b   a
 *
 * Obviously, a window size of 16 would be best. If that doesn't work, I suggest
 * a window size that divides the work evenly, preferably as large as possible:
 * 6, 4, 2.
 *
 * The validate_lbr() code in the kernel module is generic enough to support all
 * different types of window sizes. However, a window size that results in the
 * last state containing less entries than the others, requires some more static
 * analysis efforts: a copy of the valid states containing the number of
 * 'leftover' entries has to be generated.
 */
#define WINDOW_SIZE 16

/* Macro for dividing two integers and rounding up */
#define CEIL_DIV(x, y) (((x) + (y) - 1) / (y))

/* The number of hashes that we need to compute for one LBR */
#define NR_HASHES 1 + CEIL_DIV(LBR_ENTRIES - WINDOW_SIZE, WINDOW_SIZE - 1)


/*
 * Entries in state files contain more than just the LBR address:
 *
 * abcdddddddddddddxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
 * a = address relies in library
 * b = address relies in target
 * c = address is target's main
 * d = library index (13 bits)
 * x = actual address (48 bits)
 */

#define LIB_FLAG (1ULL << 63)   // address relies in library
#define TARG_FLAG (1ULL << 62)  // address relies in target
#define MAIN_FLAG (1ULL << 61)  // address is target's main()

#define ADDR_BITS 48
#define INDX_BITS 13

// macros to extract addresses and indexes
#define TO_ADDR(addr) (uint64_t)((((uint64_t)addr) << (64 - ADDR_BITS            )) >> (64 - ADDR_BITS))
#define TO_INDX(addr) (uint64_t)((((uint64_t)addr) << (64 - ADDR_BITS - INDX_BITS)) >> (64 - INDX_BITS))
// macro to set index
#define SET_INDX(addr, indx) (addr = addr | ((uint64_t) indx << ADDR_BITS))


// this one is highly speculative and only used to quick patch previous state files
#define LIBC_INDEX 6
#define PTHREAD_INDEX 1
// (as for nginx)


/* Name of the environment variable that holds the filename of the file that
 * holds valid paths.
 */
#define ENV_PATHFILE "PATHFILE"
#define ENV_LIBINDEX "LIBINDEX"

/* This struct holds a valid LBR state for WINDOW_SIZE LBR entries */
struct lbr_valid_state {
    /* The following from[] and to[] arrays may be removed if static analysis
     * generates the hash for us. However, keeping them here makes it possible
     * to (re)generate the hashes at runtime, when we know the offset of libc
     * and, in the case of position independent code, ourself. Computing the
     * hashes at runtime avoids having to do calculations in the kernel module
     * during validating, speeding up the runtime.
     */
    uint64_t from[WINDOW_SIZE];
    uint64_t   to[WINDOW_SIZE];

    uint8_t  hash[DIGEST_LENGTH];
};



/* This struct maps a (function) address (a LBR TO entry) to valid LBR states */
struct lbr_valid_state_map { 
    /* The lbr_valid_state and lbr_valid_state_map structs can be merged, but
     * separating them makes the validate_lbr() procedure much faster: instead
     * of comparing against all valid paths, the module only has to compare
     * against valid paths that end in this address. 
     */
    uint64_t               to;
    uint32_t               states;
    struct lbr_valid_state *state;
};

/* This meta-struct stores everything */
struct lbr_paths {
    uint32_t             funcs;
    struct lbr_function *func;
    
    /* The following array holds addresses of functions that have their address
     * taken. These function may be called from anywhere. Before validating the
     * LBR, we will search for entries that branch to one of these functions,
     * and, if there are any, set the src of these entries to 0x00.
     */
    uint32_t  ats;
    uint64_t *address_taken;
    
    uint32_t                    state_maps;
    struct lbr_valid_state_map *state_map;
};
#define ADDRESS_TAKEN 0xc477b4c8 /* callback */



#define MAX_LIBS 32
#define MAX_LIBNAME_LENGTH 128

/* for copying the paths to the kernel */
struct lbr_paths_kernel_copy {
    struct lbr_paths *paths;
    size_t size;
    uint64_t indirect_call_source;

    /* These are here so we can easily give them back to the analyzer. */
    uint64_t load_from;
    uint64_t armor_lib_enter;
    uint64_t armor_lib_return;
    uint64_t pthread_create;
    uint64_t pthread_create_return;
};


struct lib_index {
    unsigned int index;
    char libname[MAX_LIBNAME_LENGTH];
    uint64_t base;
};


struct wrapper_addresses {
    uint64_t libc_base;
    uint64_t load_from;
    uint64_t orig_main;
    uint64_t armor_lib_enter;
    uint64_t armor_lib_return;
    uint64_t armor_cbk_enter;
    uint64_t armor_cbk_return;
    uint64_t armor_cbk_target_call;
    uint64_t armor_cbk_target_return;
    uint64_t pthread_create_offset;
    uint64_t pthread_create;
    uint64_t pthread_create_return;
};


struct offsets_t {
    char name[MAX_LIBS][MAX_LIBNAME_LENGTH];
    uint64_t start[MAX_LIBS];
    uint64_t end[MAX_LIBS];
    uint64_t libs;  /* number of libs - VMAs to be more precise. */
   
    /* This is the LBR.source entry for *all* indirect call instructions. I
     * think you can see this ADDRESS_TAKEN. */
    uint64_t indirect_call_source;

    /* This is the LBR.source address of the first entry ever seen: from our
     * loader to the target's main(). */
    uint64_t load_from;

    uint64_t armor_lib_enter;

    /* This is LBR.source address of where all library calls return from. */
    uint64_t armor_lib_return;

    /* This is the LBR.to address of pthread_create. */
    uint64_t pthread_create;

    /* This is the LBR.source address of where pthread create returns from. */
    uint64_t pthread_create_return;
};

struct plt_got_copy_t {
    size_t size; // number of bytes of plts and gots array
    uint64_t items; // number of items in array (probably size / 8)
    uint64_t *plts;
    uint64_t *gots;
};

struct simples_t {
    size_t size; // number of bytes of exitpoints
    uint64_t items; // number of items in exitpoints (probably size / 8)
    uint64_t *exitpoints;
};



#ifndef __KERNEL__


#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <openssl/sha.h>
#include <openssl/md4.h>


/***** Function prototypes *****/

/* Serializes a given struct lbr_paths into a contiguous region of memory on the
 * heap. caller should free the result.
 */
static inline struct lbr_paths *lbr_pack_paths(struct lbr_paths *paths);

/* Rewrites the pointers of a given struct lbr_paths assuming it relies in a
 * contiguous region of memory.  This is essentially our unpacking/unserializing
 * function and is used when valid paths are read from disk.
 */
static inline struct lbr_paths *lbr_link_paths(struct lbr_paths *paths);

/* Returns the total number of bytes occupied by the given struct lbr_pahts. */
static inline size_t lbr_sizeof_paths(struct lbr_paths *paths);

/* Frees all allocations used by an unserialized struct lbr_paths */
static inline void lbr_free_paths(struct lbr_paths *paths);

/* reading and writing valid paths from and to disk */
static inline int write_paths(const char *fname, struct lbr_paths *paths);

/* user should free the result */
static inline struct lbr_paths *read_paths(const char *fname);

/* (re)compute the digest for all LBR states */
static inline void hash_paths(struct lbr_paths *paths);

/* dump valid paths on screen */
static inline void dump_paths(struct lbr_paths *paths);

/* dummy fill */
static inline void dummy_fill(struct lbr_paths *paths);

/* update the valid paths with information gathered at runtime */
//static inline void update_paths(struct lbr_paths *paths, uint64_t libc_base, uint64_t load_from, uint64_t orig_main, struct lib_wrappers * wrappers);
static inline void update_paths(struct lbr_paths *paths, 
                                struct wrapper_addresses *addresses, 
                                struct lib_index libindex[MAX_LIBS]);

static inline void *my_malloc(size_t size);
static inline void *my_calloc(size_t nmemb, size_t size);


static inline void dump_offsets(struct offsets_t *offset) {
    int i;
    printf("libs: %lu\n", offset->libs);
    for (i = 0; i < offset->libs; i++) {
        printf("%p - %p: %s\n", (void *) offset->start[i], (void *)offset->end[i], offset->name[i]);
    }

    printf("indirect call source: %p\n", (void *)offset->indirect_call_source);
    printf("load from:            %p\n", (void *)offset->load_from);
    printf("armor lib enter:      %p\n", (void *)offset->armor_lib_enter);
    printf("armor lib return:     %p\n", (void *)offset->armor_lib_return);
    printf("pthread create:       %p\n", (void *)offset->pthread_create);
    printf("pthread create return:%p\n", (void *)offset->pthread_create_return);
};

static inline void *my_malloc(size_t size) {
    void *p = malloc(size);
    if (p == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    return p;
}
static inline void *my_calloc(size_t nmemb, size_t size) {
    void *p = calloc(nmemb, size);
    if (p == NULL) {
        perror("calloc failed");
        exit(EXIT_FAILURE);
    }
    return p;
}

static inline void hexdump(uint8_t *p, size_t s) {
    int i;
    for (i = 0; i < s; i++) {
        printf("%02x", p[i]);
    }
    printf("\n");
}

static inline size_t lbr_sizeof_paths(struct lbr_paths *paths) {
    int i;
    size_t size;
    size  = sizeof(struct lbr_paths);
    size += paths->funcs      * sizeof(struct lbr_function);
    size += paths->ats        * sizeof(uint64_t);
    size += paths->state_maps * sizeof(struct lbr_valid_state_map);
    for (i = 0; i < paths->state_maps; i++) {
        size += paths->state_map[i].states * sizeof(struct lbr_valid_state);
    }
    return size;
}

#endif // ifndef __KERNEL__

/* The following two functions are also used by the kernel module. printf =
 * printk though...
 */

#ifdef __KERNEL__
#define printf printk
#endif 

static inline struct lbr_paths *lbr_link_paths(struct lbr_paths *paths) {
    uint64_t p;
    int i;

    p = (uint64_t) paths;
    p += sizeof(struct lbr_paths);

    paths->func = (struct lbr_function *) p;
    p += paths->funcs * sizeof(struct lbr_function);

    paths->address_taken = (uint64_t *) p;
    p += paths->ats * sizeof(uint64_t);

    paths->state_map = (struct lbr_valid_state_map *)p;
    p += paths->state_maps * sizeof(struct lbr_valid_state_map);

    for (i = 0; i < paths->state_maps; i++) {
        paths->state_map[i].state = (struct lbr_valid_state *) p;
        p += paths->state_map[i].states * sizeof(struct lbr_valid_state);
    }

    return paths;
}

/* Dump valid paths on screen. */
static inline void dump_paths(struct lbr_paths *paths) {
    int i, j, k;


    printf("Blacklisted functions:\n");
    for (i = 0; i < paths->funcs; i++) 
        printf("- 0x%012lx: %s\n", (long unsigned int)paths->func[i].fptr, 
                                                      paths->func[i].fname);
  
    printf("\n");

    for (i = 0; i < paths->state_maps; i++) {
        printf("%d valid LBR states to address 0x%012lx:\n",paths->state_map[i].states, 
                                        (long unsigned int) paths->state_map[i].to);

        for (j = 0; j < paths->state_map[i].states; j++) {
            for (k = 0; k < WINDOW_SIZE; k++) 
                printf(" <-- (0x%012lx,0x%012lx)\n", 
                        (long unsigned int) paths->state_map[i].state[j].from[k], 
                        (long unsigned int) paths->state_map[i].state[j].to[k]);

            printf(" Digest: ");
            for (k = 0; k < DIGEST_LENGTH; k++) 
                printf("%02x", paths->state_map[i].state[j].hash[k]);
            
            printf("\n\n");
        }
    }
}

#ifdef __KERNEL__
#undef printf
#endif

#ifndef __KERNEL__

static inline struct lbr_paths *lbr_pack_paths(struct lbr_paths *paths) {
    int i;
    size_t offset;
    uint8_t *packed;
    size_t size;

    offset = 0;
    size   = lbr_sizeof_paths(paths);
    packed = (uint8_t *)my_malloc(size);

    memcpy(packed + offset, paths, sizeof(struct lbr_paths));
    offset += sizeof(struct lbr_paths);

    memcpy(packed + offset, paths->func, paths->funcs * sizeof(struct lbr_function));
    offset += paths->funcs * sizeof(struct lbr_function);

    memcpy(packed + offset, paths->address_taken, paths->ats * sizeof(uint64_t));
    offset += paths->ats * sizeof(uint64_t);

    memcpy(packed + offset, paths->state_map, paths->state_maps * sizeof(struct lbr_valid_state_map));
    offset += paths->state_maps * sizeof(struct lbr_valid_state_map);

    for (i = 0; i < paths->state_maps; i++) {
        memcpy(packed + offset, paths->state_map[i].state, paths->state_map[i].states * sizeof(struct     lbr_valid_state));
        offset += paths->state_map[i].states * sizeof(struct lbr_valid_state);
    }

    return lbr_link_paths( (struct lbr_paths *)packed );
}


static inline void lbr_free_paths(struct lbr_paths *paths) {
    int i;
    for (i = 0; i < paths->state_maps; i++) {
        free(paths->state_map[i].state);
    }
    free(paths->state_map);
    if (paths->address_taken) free(paths->address_taken);
    if (paths->func)          free(paths->func);
}


static inline int write_paths(const char *fname, struct lbr_paths *paths) {
    FILE *fp;
    int ret;

    // generate hashes for the paths. this may be removed.
//  hash_paths(paths);
    
    // write the paths to file
    fp = fopen(fname, "w");
    if (fp == NULL) return 1;

    ret = fwrite(paths, lbr_sizeof_paths(paths), 1, fp);
    if (ret != 1) return 1;

    ret = fclose(fp);
    if (ret != 0) return 1;

    return 0;
};

/* Read the valid paths from disk. */
static inline struct lbr_paths *read_paths(const char *fname) {
    FILE *fp;
    size_t size;
    int ret;
    struct lbr_paths *paths;

    fp = fopen(fname, "r");
    if (fp == NULL) return NULL;

    // get the file size so we know how much memory we must allocate
    ret = fseek(fp, 0, SEEK_END);
    if (ret != 0) return NULL;
    
    size = ftell(fp);
    if (size == -1) return NULL;
    
    ret = fseek(fp, 0, SEEK_SET);
    if (ret != 0) return NULL;

    // allocate the memory and read the file contents
    paths = (struct lbr_paths *)my_malloc(size);

    ret = fread(paths, size, 1, fp);
    if (ret != 1) return NULL;

    ret = fclose(fp);
    if (ret != 0) return NULL;

    // update the pointers
    return lbr_link_paths(paths);
};

static inline int read_indexes(const char *fname, struct lib_index libindex[MAX_LIBS]) {
    FILE *fp;
    int i;

/* This causes some kind of loop. Weird stuff. TODO
    char cmd[1024];
    sprintf(cmd, "cut -d' ' -f4,5 %s | uniq", fname);
    printf("executing %s\n", cmd);
    fp = popen(cmd, "r");
    if (fp == NULL) return -1;
*/
    fp = fopen(fname, "r");
    if (fp == NULL) return -1;

    i = 0;
    while (fscanf(fp, "%d %s\n", &libindex[i].index, libindex[i].libname) != EOF &&
           i < MAX_LIBS) {
        i++;
    }

//  pclose(fp);
    fclose(fp);
    return 0;
}


/* (Re)compute the digest for all LBR states. */
static inline void hash_paths(struct lbr_paths *paths) {
    int i, j, k;

//  SHA_CTX ctx;
    MD4_CTX ctx;

    for (i = 0; i < paths->state_maps; i++) {
        for (j = 0; j < paths->state_map[i].states; j++) {
//          SHA1_Init(&ctx);
            MD4_Init(&ctx);
            for (k = 0; k < WINDOW_SIZE; k++) {
//              SHA1_Update(&ctx, &paths->state_map[i].state[j].from[k], sizeof(uint64_t));
//              SHA1_Update(&ctx, &paths->state_map[i].state[j].to[k], sizeof(uint64_t));
                MD4_Update(&ctx, &paths->state_map[i].state[j].from[k], sizeof(uint64_t));
                MD4_Update(&ctx, &paths->state_map[i].state[j].to[k], sizeof(uint64_t));
            }
//          SHA1_Final(paths->state_map[i].state[j].hash, &ctx);
            MD4_Final(paths->state_map[i].state[j].hash, &ctx);
        }
    }
}


/* Update the LBR states given some more information. */
static inline void update_paths(struct lbr_paths *paths, 
                                struct wrapper_addresses *addresses,
                                struct lib_index libindex[MAX_LIBS]) {
    int i, j, k;
    uint64_t static_main;
    uint64_t offset;


    /* After static analysis, the LBR.from address that calls the target's
     * main() is unknown and set to 0. This means that the provided valid paths
     * may contain a number of <from: 0x0, to: main> tuples.
     * Now that, at runtime, we have the address from where main() is invoked,
     * we can update the valid paths.
     */
    
    /* This will hold the address of main() as computed during static analysis */
    static_main = 0;

    /* The offset when target is position indepedent */
    offset = 0;


    /* First round. search for the target's main() as computed during static
     * analysis and compare against the real address of main. If this differs,
     * the binary is position indepedent and we'll have to update all it's
     * addresses which is done in the second round.
     * While we're at it (search for <from: 0x0, to: main>), set the from
     * address for these entries.
     */
    for (i = 0; i < paths->state_maps; i++) {
      for (j = 0; j < paths->state_map[i].states; j++) {
        for (k = 0; k < WINDOW_SIZE; k++) {
          if (paths->state_map[i].state[j].from[k] == 0 && 
              paths->state_map[i].state[j].to  [k] & MAIN_FLAG) {
            /* <from: 0x0, to: main> */
            static_main = TO_ADDR(paths->state_map[i].state[j].to[k]);
            printf("static_main: 0x%lx\n", static_main);
            printf("  orig_main: 0x%lx\n", addresses->orig_main);

            if (static_main != addresses->orig_main) {
              /* static analysis failed to compute the target's main()
               * address correctly. assuming position independent code.
               */
               offset = addresses->orig_main - static_main;
               printf("offset: 0x%lx (%lu)\n",offset,offset);
               /* TODO: add a check to make sure no different offsets are found? */
            }
            
            paths->state_map[i].state[j].from[k] = addresses->load_from;
          }
        }
      }
    }


    /* array for fast access */
    uint64_t base[MAX_LIBS];
    memset(base, 0, sizeof(base));
    i = 0;
    
    while (libindex[i].index != 0) {
        base[libindex[i].index] = libindex[i].base;
        i++;
    }
    base[0] = 0x00; // so that we skip an if statement

    /* Second round. Update all LBR entries that are currently in position
     * independent mode. If the offset is set, entries that rely in
     * the target are also updated.
     */
    for (i = 0; i < paths->state_maps; i++) {
      if (paths->state_map[i].to & TARG_FLAG) {
        paths->state_map[i].to += offset; 
      }

//    if (paths->state_map[i].to & LIB_FLAG) {
        paths->state_map[i].to += base[ TO_INDX(paths->state_map[i].to) ];
//    }
      for (j = 0; j < paths->state_map[i].states; j++) {
        for (k = 0; k < WINDOW_SIZE; k++) {
          if (paths->state_map[i].state[j].from[k] & TARG_FLAG) {
            paths->state_map[i].state[j].from[k] += offset;
          }
          if (paths->state_map[i].state[j].to[k]   & TARG_FLAG) {
            paths->state_map[i].state[j].to[k]   += offset;
          }
          if (TO_ADDR(paths->state_map[i].state[j].from[k]) == ADDRESS_TAKEN) 
            paths->state_map[i].state[j].from[k] = addresses->armor_cbk_target_call;
          if (paths->state_map[i].state[j].from[k] & LIB_FLAG) {
            // there must be a way to avoid this if construction... TODO
            if (TO_ADDR(paths->state_map[i].state[j].from[k]) == addresses->pthread_create_offset) 
                paths->state_map[i].state[j].from[k] = addresses->pthread_create_return;
            else
                paths->state_map[i].state[j].from[k] = addresses->armor_lib_return;
//          paths->state_map[i].state[j].from[k] += addresses->libc_base;
          }
//        if (paths->state_map[i].state[j].to[k]   & LIB_FLAG) {
            // there must be a way to avoid this if construction... TODO
            if (TO_ADDR(paths->state_map[i].state[j].to[k]) == addresses->pthread_create_offset) 
                paths->state_map[i].state[j].to[k] = addresses->pthread_create;
            else
                paths->state_map[i].state[j].to[k]   += base[ TO_INDX(paths->state_map[i].state[j].to[k]) ]; 
//        }
        }
      }
    }
    for (i = 0; i < paths->funcs; i++) {
      if (paths->func[i].fptr & TARG_FLAG) {
        paths->func[i].fptr += offset;
      }
//    if (paths->func[i].fptr & LIB_FLAG) {
        paths->func[i].fptr += base[ TO_INDX(paths->func[i].fptr) ];
//    }
    }

    /* Third round, remove the bits that indicate the address' type. */
    for (i = 0; i < paths->state_maps; i++) {
      paths->state_map[i].to = TO_ADDR(paths->state_map[i].to);
      for (j = 0; j < paths->state_map[i].states; j++) {
        for (k = 0; k < WINDOW_SIZE; k++) {
          paths->state_map[i].state[j].from[k] = TO_ADDR(paths->state_map[i].state[j].from[k]);
          paths->state_map[i].state[j].to  [k] = TO_ADDR(paths->state_map[i].state[j].  to[k]);
        }
      }
    }
    for (i = 0; i < paths->funcs; i++) {
      paths->func[i].fptr = TO_ADDR(paths->func[i].fptr);
    }

    /* Final round. Compute the hashes. */
    hash_paths(paths);

}






/* Fill a given lbr_paths struct with some dummy values. Assuming WINDOW_SIZE == 4 */
static inline void dummy_fill(struct lbr_paths *paths) {
    paths->func[0].fptr = 0xc2170 | LIB_FLAG;
    strncpy(paths->func[0].fname,"execve",6);
    paths->funcs++;
/*
    paths->func[1].fptr = 0xc0ff3333;
    strncpy(paths->func[1].fname,"getpid",6);
    paths->funcs++;
 */
    /* valid paths to 0xc2170 (execve in my libc) */
    paths->state_map[0].to = 0xc2170 | LIB_FLAG;
    paths->state_map[0].state[0].from[0] = 0x400470 | TARG_FLAG; // execve@plt
    paths->state_map[0].state[0].  to[0] = 0xc2170  | LIB_FLAG; // libc execve
    paths->state_map[0].state[0].from[1] = 0x00;                 // loader, unknown
    paths->state_map[0].state[0].  to[1] = 0x40059a | TARG_FLAG; // main, 1st instruction after call puts@plt;
    paths->state_map[0].state[0].from[2] = 0x400450 | TARG_FLAG; // puts@plt
    paths->state_map[0].state[0].  to[2] = 0x70c70  | LIB_FLAG;  // libc puts
    paths->state_map[0].state[0].from[3] = 0x00;    // loader, unknown
    paths->state_map[0].state[0].  to[3] = 0x40052d | TARG_FLAG | MAIN_FLAG; // main
    paths->state_map[0].states++;

    /* The following path to execve is for the most simple nothing.c file:
     * 
     * int main(int argc, char **argv, char **envp) {
     *    execve(argv[1], &argv[1], NULL);
     * }
     */
    paths->state_map[0].state[1].from[0] = 0x400420 | TARG_FLAG; // execve@plt
    paths->state_map[0].state[1].to  [0] = 0xc2170  | LIB_FLAG; // libc execve
    paths->state_map[0].state[1].from[1] = 0x00;                 // loader, unknown
    paths->state_map[0].state[1].to  [1] = 0x40052d | TARG_FLAG | MAIN_FLAG; // main
    paths->state_map[0].state[1].from[2] = 0x00;
    paths->state_map[0].state[1].  to[2] = 0x00;
    paths->state_map[0].state[1].from[3] = 0x00;
    paths->state_map[0].state[1].  to[3] = 0x00;
    paths->state_map[0].states++;
    paths->state_maps++;
    
    
    
    /* valid paths to 0x40057d (main) */
    paths->state_map[1].to = 0x40052d | TARG_FLAG | MAIN_FLAG;
    paths->state_map[1].state[0].from[0] = 0x00; // loader, unkown
    paths->state_map[1].state[0].  to[0] = 0x40052d | TARG_FLAG | MAIN_FLAG;  // main
    paths->state_map[1].state[0].from[1] = 0x00;
    paths->state_map[1].state[0].  to[1] = 0x00;
    paths->state_map[1].state[0].from[2] = 0x00;
    paths->state_map[1].state[0].  to[2] = 0x00;
    paths->state_map[1].state[0].from[3] = 0x00;
    paths->state_map[1].state[0].  to[3] = 0x00;
    paths->state_map[1].states++;
    paths->state_maps++;
    
    /* valid paths to 0x00 */
    paths->state_map[2].to = 0x0;
    paths->state_map[2].state[0].from[0] = 0x00;
    paths->state_map[2].state[0].  to[0] = 0x00;
    paths->state_map[2].state[0].from[1] = 0x00;
    paths->state_map[2].state[0].  to[1] = 0x00;
    paths->state_map[2].state[0].from[2] = 0x00;
    paths->state_map[2].state[0].  to[2] = 0x00;
    paths->state_map[2].state[0].from[3] = 0x00;
    paths->state_map[2].state[0].  to[3] = 0x00;
    paths->state_map[2].states++;
    paths->state_maps++;

/*
    paths->state_map[0].state[1].from[0] = 0x07;
    paths->state_map[0].state[1].  to[0] = 0xdeadbeef;
    paths->state_map[0].state[1].from[1] = 0x09;
    paths->state_map[0].state[1].  to[1] = 0x10;
    paths->state_map[0].state[1].from[2] = 0x11;
    paths->state_map[0].state[1].  to[2] = 0x12;
    paths->state_map[0].state[1].from[3] = 0x13;
    paths->state_map[0].state[1].  to[3] = 0x08;
    paths->state_map[0].states++;

    paths->state_map[0].state[2].from[0] = 0x00;
    paths->state_map[0].state[2].  to[0] = 0xdeadbeef;
    paths->state_map[0].state[2].from[1] = 0x00;
    paths->state_map[0].state[2].  to[1] = 0x00;
    paths->state_map[0].state[2].from[2] = 0x00;
    paths->state_map[0].state[2].  to[2] = 0x00;
    paths->state_map[0].state[2].from[3] = 0x00;
    paths->state_map[0].state[2].  to[3] = 0x00;
    paths->state_map[0].states++;
    paths->state_maps++;
 */
    /* valid paths to 0x08 */
/*  paths->state_map[1].to = 0x08;
    paths->state_map[1].state[0].from[0] = 0x00;
    paths->state_map[1].state[0].  to[0] = 0x08;
    paths->state_map[1].state[0].from[1] = 0x00;
    paths->state_map[1].state[0].  to[1] = 0x00;
    paths->state_map[1].state[0].from[2] = 0x00;
    paths->state_map[1].state[0].  to[2] = 0x00;
    paths->state_map[1].state[0].from[3] = 0x00;
    paths->state_map[1].state[0].  to[3] = 0x00;
    paths->state_map[1].states++;
    paths->state_maps++;
 */


    /* valid paths to 0xc0ff3333 */
/*  paths->state_map[3].to = 0xc0ff3333;
    paths->state_map[3].state[0].from[0] = 0x00;
    paths->state_map[3].state[0].  to[0] = 0xc0ff3333;
    paths->state_map[3].state[0].from[1] = 0x00;
    paths->state_map[3].state[0].  to[1] = 0x00;
    paths->state_map[3].state[0].from[2] = 0x00;
    paths->state_map[3].state[0].  to[2] = 0x00;
    paths->state_map[3].state[0].from[3] = 0x00;
    paths->state_map[3].state[0].  to[3] = 0x00;
    paths->state_map[3].states++;
    paths->state_maps++;
 */
    hash_paths(paths);
}




#endif /* __KERNEL__ */






#endif /* __LBR_STATE__ */


