#include <sys/time.h>
#include <pass.h>

#include <sys/ioctl.h>
#include <fcntl.h>

#include "armor.h"

#include <dynProcess.h>
#include <Symtab.h>
#include <AddrLookup.h>
#include <common/opt/passi.h>

PASS_ONCE();


#include <PatchModifier.h>


using namespace Dyninst::PatchAPI;
using namespace Dyninst::InstructionAPI;
using namespace Dyninst::SymtabAPI;

#include "lbr-state.h"
#include <errno.h>

/* name of functions in shared library that will disable and enable the lbr during library calls */
#define ARMOR_LIB_ENTER     "armor_lib_enter"
#define ARMOR_CBK_ENTER     "armor_cbk_enter"

#define MAX_CALL_INSN_SIZE 10

FILE *fp;


/* This Dyninst pass will instrument the target program so that library
 * code is not tracked by the LBR. We do this by wrapping each used library
 * function (those found in the PLT) with the ARMOR_LIB_ENTER function. This
 * function will make the appropriate ioctl()'s to the kernel interface in order
 * to enable or disable LBR tracking.
 * To avoid instrumenting multiple exit points, the ARMOR_LIB_ENTER function
 * does a call() to the original library. This adds an additional return address
 * to the stack, potentially breaking stack frames. We solve this by passing the
 * original return address to the kernel module which will restore it once the
 * second ioctl() is made.
 * 
 * Indirect calls from the target binary could also target library code. Also,
 * indirect calls inside library code may target functions from the binary. We
 * thus have to instrument these instructions as well.
 *
 * Note that our current method of instrmenting library functions relies on
 * Dyninst to correctly disassemble the entire lib function. Due to code
 * optimizations, this may not always be the case, however. We identify these
 * situations and pass information about this to the JIT analyzer. Note that
 * this is not a fundamental PathArmor issue: instrumentation should have been
 * implemented by simply updating the PLT. 
 *
 * To gain some speed performance, we do not instrument 'simple' library
 * functions. Simple functions are those that do not trigger any kind of
 * indirect branch and thus do not pollute the LBR: only two edges will show up,
 * entry and exit. We pass the entry and possible entry points to the JIT
 * analyser.
 *
 * Note that for clearity, we removed an alternative implementation variant
 * where we would not wrap library functions, but instead add instrumentation
 * twice: both at the entry and then all the exit points.
 */



BPatch_function *findFunc(BPatch_image *image, const char *name) {
    std::vector<BPatch_function*> funcs;

    image->findFunction(name, funcs);
    if (funcs.empty()) {
        fprintf(fp,"Could not find any function named %s\n",name);
        exit(EXIT_FAILURE);
    }
    if (funcs.size() > 1) {
        fprintf(fp,"Found more than one function named %s\n",name);
        exit(EXIT_FAILURE);
    }
    return funcs[0];
}

bool isSimple(BPatch_function *f) {
    std::vector<BPatch_point *> *calls = f->findPoint(BPatch_subroutine);
    if (calls && calls->size() > 0) return false;
    return true;
}

std::vector<relocationEntry> getPLT(BPatch_image *image) {
    /* Find the default module (the mutatee). */
    BPatch_module *default_module;
   
    default_module = image->findModule("DEFAULT_MODULE");
    /* exception for pure-ftpd... */
    if (default_module == NULL) default_module = image->findModule("pure-ftpd");
    if (default_module == NULL) {
        fprintf(fp,"ERROR: Could not find default module\n");
        exit(EXIT_FAILURE);
    }

    /* Get the symtab of this module.*/
    SymtabAPI::Module *sym_module = SymtabAPI::convert(default_module);
    SymtabAPI::Symtab *symtab = sym_module->exec();

    std::vector<relocationEntry> plt;
    symtab->getFuncBindingTable(plt);
    return plt;
}


Symbol *getSymbol(BPatch_module *mod, uint64_t offset) {

        SymtabAPI::Module *sym_module = SymtabAPI::convert(mod);

        std::vector<Symbol *> symbols;
        if (!sym_module->getAllSymbols(symbols)) return NULL;

        for (std::vector<Symbol *>::iterator it  = symbols.begin();
                                             it != symbols.end();
                                           ++it) {
            Symbol *symbol = *it;
            if (symbol->getOffset() == offset) return symbol;
        }
        return NULL;
}


void wrapLibraries(BPatch_image *image, std::vector<BPatch_module *> *mods, BPatch_addressSpace *as) {
    
    /* Our snippets will call ourselves, but we do our own checks to prevent
     * recursion. We can safely disable Dyninst's recusion protection.
     */
    bpatch.setTrampRecursive(true);

    std::vector<BPatch_snippet*> args;
    /* Create the library wrapper snippet by loading the ARMOR_LIB_ENTER
     * function as defined in the shared library in libwrappers.so. This shared
     * library must have been pre-loaded already (in order to get a wrapper for
     * the target's main()), so this function should be available.
     */
    BPatch_function *lib_enter = findFunc(image, ARMOR_LIB_ENTER);

// TODO: get rid of DIY?
    BPatch_funcCallExpr lib_enter_call(*lib_enter, args, true);     // DIY mode
    
    /* Get a low level process representation so we can read values from the
     * mutatee's address space. */
    BPatch_process *proc = dynamic_cast<BPatch_process*>(as);
    PCProcess *llproc = proc->lowlevel_process();

    /* Get the PLT. */
    std::vector<relocationEntry> plt = getPLT(image);
    fprintf(fp,"  * Found %lu PLT entries that we have to wrap:\n", plt.size());

    /* Keep track of a set of addresses that we successfully wrapped. */
    std::set<uint64_t> wrapped_functions;
    std::set<uint64_t> got;

    /* We will wrap them all at once. */
    as->beginInsertionSet();

    struct plt_got_copy_t plt_got_copy;
    plt_got_copy.plts  = (uint64_t *) malloc( plt.size() * sizeof(uint64_t));
    plt_got_copy.gots  = (uint64_t *) malloc( plt.size() * sizeof(uint64_t));
    plt_got_copy.size  = plt.size() * sizeof(uint64_t);
    plt_got_copy.items = plt.size();
        
    /* keep track of exit points for simple functions */
    std::set<uint64_t> simple_rets;

    
    /* Loop over all PLT entries. */
    for (std::vector<relocationEntry>::iterator it  = plt.begin();
                                                it != plt.end();
                                              ++it) {
        relocationEntry rel = *it;

        /* Extract the GOT value for this PLT entry. */
        uint64_t lib_func_address;
        llproc->readDataSpace( (void *)rel.rel_addr(), sizeof(lib_func_address), 
                                                             &lib_func_address, true);
        got.insert(lib_func_address);
       
        fprintf(fp,"    o 0x%06lx->0x%06lx->0x%012lx: %-20s ", (uint64_t) rel.target_addr(), 
                                                               (uint64_t) rel.rel_addr(),
                                                               lib_func_address, rel.name().c_str());
        fflush(fp);

        plt_got_copy.plts[ it - plt.begin() ] = (uint64_t) rel.target_addr();
        plt_got_copy.gots[ it - plt.begin() ] = (uint64_t) lib_func_address;

        /* We will wrap functions only once. */        
        if ( std::find(wrapped_functions.begin(), 
                       wrapped_functions.end(), 
                           lib_func_address) != wrapped_functions.end()) {
            fprintf(fp,"    >> is already wrapped <<\n");
            continue;
        }
        /* pthread_create() required some manual work and is wrapped via
         * LD_PRELOAD */
        if (rel.name() == "pthread_create") {
            fprintf(fp,"    >> manually wrapped <<\n");
            continue;
        }
        /* Support for setjmp/longjmp primitives is left as an exercise for the
         * reader. I do not remember why select() was causing issues. It might
         * 'just work'. TODO */
        if (rel.name() ==  "siglongjmp" ||
            rel.name() ==     "longjmp" || 
            rel.name() == "__sigsetjmp" || 
            rel.name() == "select" ||
            rel.name() ==     "_setjmp") {
            
            plt_got_copy.plts[ it - plt.begin() ] |= (1ULL << 63);
            plt_got_copy.gots[ it - plt.begin() ] |= (1ULL << 63);
            
            fprintf(fp,"    >> is not supported <<\n");
            continue;
        }

        /* Populate some data fields. */
        std::vector<BPatch_function *> lib_funcs;
        if (!image->findFunction((Address) lib_func_address, lib_funcs)) {
            if (rel.name() == "time") {
                /* time is in vdso, which dyninst cannot instrument (?). it is a
                 * simple function, 21 bytes on ubuntu 14.04 LTS. */
                simple_rets.insert( (uint64_t) lib_func_address + 21 );
                fprintf(fp,"    >> simple, exit point at: %p <<\n", (void *) ((uint64_t)lib_func_address + 21));
            } else if (rel.name() == "gettimeofday") {
                /* timeofday is also in vdso. Seems to be simple as well. 282
                 * bytes on ubuntu 14.04 LTS. */
                simple_rets.insert( (uint64_t) lib_func_address + 282 );
                fprintf(fp,"    >> simple, exit point at: %p <<\n", (void *) ((uint64_t)lib_func_address + 282));
            } else {
                fprintf(fp,"    >> not found <<\n");
            }
            continue;
        }
        BPatch_function *lib_func = lib_funcs[0];
        BPatch_module   *lib      = lib_func->getModule();
        uint64_t lib_base         = (uint64_t) lib->getBaseAddr();
        uint64_t lib_func_offset  = lib_func_address - lib_base;    
        fprintf(fp,"+0x%06lx: ", lib_func_offset); 
        fflush(fp);
        Symbol *symbol = getSymbol(lib, lib_func_offset);
        if (symbol == NULL) {
            fprintf(fp,"    >> no symbol found <<\n");
            continue;
        }
        fprintf(fp,"%-25s ", symbol->getMangledName().c_str());

        if (!lib_func->isInstrumentable()) {
            plt_got_copy.plts[ it - plt.begin() ] |= (1ULL << 63);
            plt_got_copy.gots[ it - plt.begin() ] |= (1ULL << 63);

            fprintf(fp,"    >> not instrumentable, whitelisting <<\n");
            continue;
        }

        std::vector<BPatch_point *> *exit_points;

        /* Do not wrap functions that cause no indirect branch. */
        if (isSimple(lib_func)) {
            
            /* Collect exit points (address of return instructions) for these
             * functions. These will be passed to JIT so it can normalize edges
             * containing these addresses as if they originate from our library
             * wrapper. 
             */
            exit_points = lib_func->findPoint(BPatch_exit);
            if (!exit_points || exit_points->size() == 0) {
                fprintf(fp,"    >> simple, but no exit points? <<\n");
                continue;
            }

            fprintf(fp,"    >> simple, exit points at: ");
            for (std::vector<BPatch_point*>::iterator it  = exit_points->begin(); 
                                                      it != exit_points->end(); 
                                                           ++it) {
                fprintf(fp,"%p ",(*it)->getAddress());
                simple_rets.insert( (uint64_t) (*it)->getAddress() );
            }
            fprintf(fp,"<<\n");
            continue;
        }
        fprintf(fp,"\n");


        /* Do not wrap functions that never return. */
        exit_points = lib_func->findPoint(BPatch_exit);
        if (!exit_points || exit_points->size() == 0) {
            /* This could happen, but we don't want to wrap these... */
            fprintf(fp,"    >> no exit point found <<\n");
            continue;
        }

        /*****************************************************************
         * This is where the actual instrumentation takes place.
         */

        /* Get the entry pointcut */
        std::vector<BPatch_point *> *points = lib_func->findPoint(BPatch_entry);
        if (!points || points->size() == 0) {
            fprintf(fp,"ERROR: no entry point found for %s (offset: 0x%06lx)\n", 
                                            lib_func->getName().c_str(), lib_func_offset);
            exit(EXIT_FAILURE);
        } 
        if (points->size() > 1) {
            fprintf(fp,"ERROR: more than one entry point found\n");
            exit(EXIT_FAILURE);
        }

        /* Search and remove (??) existing snippets */
        std::vector<BPatchSnippetHandle*> snippets = (*points)[0]->getCurrentSnippets(BPatch_callBefore);
        if (snippets.size() > 0) {
            fprintf(fp,"      -> WARNING: %lu snippet(s) installed already!\n", snippets.size());
            for (std::vector<BPatchSnippetHandle*>::iterator it  = snippets.begin(); 
                                                             it != snippets.end(); 
                                                           ++it) {
                if (!as->deleteSnippet( *it )) {
                    fprintf(fp,"ERROR: could not to delete snippet\n");
                    exit(EXIT_FAILURE);
                }
            }
        }

        /* Insert the regular library enter snippet. */
        if (!as->insertSnippet(lib_enter_call, *points, BPatch_callBefore)) {
            fprintf(fp,"ERROR: failed to insert snippet at entry point\n");
            exit(EXIT_FAILURE);
        }
        wrapped_functions.insert(lib_func_address);

    } // loop over PLT entries
    
    /* Commit all modifications */
    if (!as->finalizeInsertionSet(true)) {
        fprintf(fp,"ERROR: failed to insert snippets\n");
        exit(EXIT_FAILURE);
    }
    fprintf(fp,"  * Modifications succesful\n");

    /* print summary regarding unwrapped GOT entries */
    for (std::set<uint64_t>::iterator it  = got.begin();
                                      it != got.end();
                                    ++it) {
        if ( std::find(wrapped_functions.begin(), 
                       wrapped_functions.end(), 
                           (*it)) == wrapped_functions.end()) {
            fprintf(fp,"  * WARNING. This GOT entry was not wrapped: %lx\n", (*it));
        }
    }

    simples_t simples;
    simples.size = simple_rets.size() * sizeof(uint64_t);
    simples.items = simple_rets.size();
    simples.exitpoints = (uint64_t *) malloc( simples.size );
    
    int i = 0;
    for (std::set<uint64_t>::iterator it  = simple_rets.begin();
                                      it != simple_rets.end();
                                    ++it, i++) {
        simples.exitpoints[ i ] = *it;
    }

    int fd = open("/dev/armor", O_RDONLY);
    if (fd < 0) {
        perror("Could not open /dev/armor");
        exit(EXIT_FAILURE);
    }

    fprintf(fp,"  * Pushing plt data to kernel module\n");
    int ret = ioctl(fd, ARMOR_IOC_PUSH_PLTS, &plt_got_copy);
    if (ret) {
        perror("Could not push plt data\n");
        exit(EXIT_FAILURE);
    }

    fprintf(fp,"  * Pushing exit points to kernel module\n");
    ret = ioctl(fd, ARMOR_IOC_PUSH_EXITS, &simples);
    if (ret) {
        perror("Could not push exit points\n");
        exit(EXIT_FAILURE);
    }
    fprintf(fp,"  * Done\n");
    
    close(fd);
}




void wrapIndirects(BPatch_image *image, std::vector<BPatch_module *> *mods, BPatch_addressSpace *as) {

    /* Loop over all modules (shared libraries) */
    for (unsigned i = 0; i < mods->size(); i++) {
          char modname[512];
        size_t modname_len = 511;
        (*mods)[i]->getFullName(modname, modname_len);
        
        /* We are not interested in stuff that is from the mutatee. */
        if ( !(*mods)[i]->isSharedLib() ) continue;

        /* Skip these: */
        if (!strcmp(modname, "libwrappers.so") ||
            !strcmp(modname, "libdyninstAPI_RT.so.9.0") ||
            !strcmp(modname, "libdyninstAPI_RT.so.8.2")) {
            fprintf(fp,"  * Skipping %s\n", modname);
            continue;
        }
        /* Support for the dynamic linker is left as an exercise. TODO */
        if (!strcmp(modname, "libdl.so.2")) {
            fprintf(fp,"  * Skipping %s << NOT SUPPORTED!\n", modname);
            continue;
        }

        fprintf(fp,"  * Searching for indirect calls in %s\n", modname);
        BPatch_module *lib = (*mods)[i];
        std::vector<BPatch_function *> *lib_functions = lib->getProcedures();
    
        bpatch.setTrampRecursive(true);
        bpatch.setSaveFPR(false);
        bpatch.setInstrStackFrames(false);
        bpatch.setLivenessAnalysis(true);

        /* Get the callback wrapper function by loading ARMOR_CBK_ENTER() */
        BPatch_function *cbk_enter = findFunc(image, ARMOR_CBK_ENTER);

        /* We will wrap them all at once... */
        as->beginInsertionSet();

        /* Loop over all library functions. */
        for (std::vector<BPatch_function *>::iterator it  = lib_functions->begin(); 
                                                      it != lib_functions->end(); 
                                                    ++it) {
            BPatch_function *func = *it;
            
            /* Some of these were causing issues at some point. I am not sure
             * why exactly and this may have been resolved already... TODO */
            if (func->getName().find("_dl") != std::string::npos) {
                fprintf(fp,"    o Skipping %s\n", func->getName().c_str());
                continue;
            }

            /* start_thread() in libpthread.so.0 does a callq *%fs:0x640 which
             * Dyninst cannot handle properly and thus causes a segfault. To
             * avoid this, we have a special LD_PRELOAD wrapper for
             * pthread_create() that replaces the target thread with a dummy
             * thread. 
             * We thus do not instrument start_thread() here.
             */
            if (!strcmp(modname, "libpthread.so.0") &&
                        func->getName() == "start_thread") {
                fprintf(fp,"    o Skipping %s\n", func->getName().c_str());
                continue;
            }

            /* Same as with _dl* functions, the following is likely a debugging
             * remainder. TODO */
            if (!strcmp(modname, "libcrypto.so.1.0.0") ) {
                if (func->getName() == "CRYPTO_malloc_locked" ||
                      func->getName() == "CRYPTO_malloc"         ||
                      func->getName() == "CRYPTO_realloc"        ) {
                    fprintf(fp,"    o Skipping %s\n", func->getName().c_str());
                    continue;
                }
            }


            /* Find all pointcuts where a subroutine is invoked. */
            std::vector<BPatch_point *> *lib_calls = func->findPoint(BPatch_subroutine);

            /* Loop over these pointcuts. */
            for (std::vector<BPatch_point *>::iterator ip = lib_calls->begin();
                                                       ip != lib_calls->end();
                                                     ++ip) {
                BPatch_point *point = *ip;
                   
                /* BPatch_point.getInsnAtPoint() doesn't work :( */
                PatchBlock *pblock = PatchAPI::convert( point->getBlock() );
                Instruction::Ptr iptr = pblock->getInsn( (Address) point->getAddress() );


                /* We are only interested in indirect call sites. */
                if (point->isDynamic()) {

                    /* We will insert a call to ARMOR_CBK_ENTER before the
                     * indirect call instruction. This function will check
                     * whether the target of the indirect call lies within the
                     * target's address space and returns directly if it does
                     * not. If it does, it will make the indirect call for us
                     * and return directly after the instrumented call
                     * instruction.
                     * For this to work, we need to pass two parameters:
                     * - The target of the indirect call instruction, so we can
                     *   do the test and make the call from there if necessary.
                     * - The size (in bytes) of the indirect call instruction so
                     *   we know exactly where to return to.
                     */

                    /* First compute the instruction size. For this, we use the
                     * hidden API function getCallFalThroughAddr(). We are not
                     * supossed to use this, but it seems to work fine for this
                     * situation.
                     */
                    uint64_t insn_size = (uint64_t) point->getCallFallThroughAddr() - 
                                         (uint64_t) point->getAddress();
                    /* Let's do a sanity check here. */ 
                    if (insn_size > MAX_CALL_INSN_SIZE) {
                        fprintf(fp,"ERROR: computed instruction size is too large\n");
                        exit(EXIT_FAILURE);
                    }
                   
                    /* Now we can setup the argument vector... */
                    std::vector<BPatch_snippet*> args;
                    BPatch_snippet* dte = new BPatch_dynamicTargetExpr();
                    args.push_back(dte);
                    BPatch_snippet* isz = new BPatch_constExpr(insn_size);
                    args.push_back(isz);
    
                    /* ...and construct and insert the snippet. */
                    BPatch_funcCallExpr cbk_enter_call( *cbk_enter, args);
                    if (!as->insertSnippet(cbk_enter_call, *point, BPatch_callBefore)) {
                        fprintf(fp,"ERROR: failed to insert snippet at callback pointcut\n");
                        exit(EXIT_FAILURE);
                    }
                } // if point->isDynamic()
            } // loop over pointcuts
        } // loop over library functions
        /* Commit patches. */
        fprintf(fp,"  * Committing patches for %s...\n", modname); 
        if (!as->finalizeInsertionSet(true)) {
            fprintf(fp,"ERROR: failed to insert snippets\n");
            exit(EXIT_FAILURE);
        }

    } // loop over libraries

    fprintf(fp,"  * All indirect calls have been wrapped\n");
}


namespace {

  class PadynPass : public ModulePass {

    public:
        static char ID;
        PadynPass() : ModulePass(ID) {}

        virtual bool runOnModule(void *M) {
            BPatch_addressSpace *as = (BPatch_addressSpace*) M;

            fp = fdopen( dup (fileno (stderr)), "w");

            /* here we go! */
            fprintf(fp,"- getImage()...\n");
            BPatch_image *image = as->getImage();

            fprintf(fp,"- getModules()...\n");
            std::vector<BPatch_module *> *mods = image->getModules();
            
#ifdef ARMOR_WRAP_LIBRARIES
            fprintf(fp,"- wrapping library calls...\n");
            wrapLibraries(image, mods, as);
#endif

#ifdef ARMOR_WRAP_INDIRECTS
            fprintf(fp,"- wrapping indirect calls...\n");
            wrapIndirects(image, mods, as);
#endif

            fprintf(fp,"- Finished Dyninst setup, returning to target\n\n");
            fclose(fp);
            
            return false;
        }
  };
}

char PadynPass::ID = 0;
RegisterPass<PadynPass> MP("padyn", "Padyn Pass");

