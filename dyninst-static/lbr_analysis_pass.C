#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include <iostream>
#include <string>
#include <sys/time.h>
#include <sys/ioctl.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <string.h>

#include "BPatch.h"
#include "BPatch_addressSpace.h" 
#include "BPatch_process.h" 
#include "BPatch_object.h"
#include "BPatch_binaryEdit.h" 
#include "BPatch_function.h"
#include "BPatch_point.h"
#include "BPatch_flowGraph.h" 
#include "BPatch_memoryAccessAdapter.h"

#include "PatchCommon.h"
#include "PatchMgr.h"
#include "PatchModifier.h"

#include "Register.h"

#include "lbr-state.h"

using namespace std;
using namespace Dyninst;
using namespace Dyninst::PatchAPI;
using namespace Dyninst::ParseAPI;
using namespace Dyninst::InstructionAPI;
using namespace Dyninst::SymtabAPI;

#include "function.h"
#include "instPoint.h"

#include "env.h"
#include "defs.h" 
#include "arms_utils.h" 
#include "arms_bb.h"
#include "arms_edge.h" 
#include "arms_cfg.h" 
#include "arms_dyninst_cfg.h"

#include "armor.h"

#include "address_taken_parser.h"

#include "lbr-state.h"
#include "lbr_analysis.h"

#include <pass.h>

#define JIT_DEBUG 1
#if JIT_DEBUG
  #define jit_printf(...) do { fprintf(stdout, __VA_ARGS__); } while(0)
#else
  #define jit_printf(...) do {} while(0)
#endif

PASS_ONCE();

static cl::opt<std::string>
optOutput("output",
  cl::desc("Define output file concat(binary path, <output file>)."),
  cl::init(""));

static cl::opt<bool>
optVerbose("v", 
  cl::desc("Enable/disable verbose output."),
  cl::init(false));

static cl::opt<std::string>
optStats("stats",
  cl::desc("Write statistics to file concat(binary path, <stats file>)."),
  cl::init(""));

static cl::opt<std::string>
optICallMapType("icall-map-type",
  cl::desc("Specify map type used by icall resolver."),
  cl::init("none")); /* at, params, type, naive, none */

static cl::opt<std::string>
optAddressTakenPolicy("address-taken-policy",
  cl::desc("Specify which address taken policy to apply."),
  cl::init("default")); /* default, cfg, bin-cfi, lockdown */

static cl::opt<int>
optWindowSize("window-size",
  cl::desc("LBR window size."),
  cl::init(2));

static cl::opt<bool>
optDaemon("daemon", 
  cl::desc("Enable/disable daemon mode."),
  cl::init(false));

static cl::opt<bool>
optCFGStats("cfg-stats", 
  cl::desc("Dump CFG stats and exit."),
  cl::init(false));

namespace {
  int armor_fd   = -1;
  volatile int run_daemon =  1;

  void sigint_handler(int sig)
  {
    if(sig == SIGINT && optDaemon) {
      printf("\nSIGINT caught, exiting\n");
      stat_dump_finals();
      fflush(stdout);
      fflush(stderr);
      run_daemon = 0;

      /* Exiting directly to avoid endless loop. */
      exit(EXIT_FAILURE);
    }
  }


  class LBR_AnalysisPass : public ModulePass {

  public:
    static char ID;
    LBR_AnalysisPass() : ModulePass(ID) {}

    virtual bool runOnModule(void *M) {
      bool is_bin_edit;
      const char *bin, *path, *output, *stats;
      BPatch bpatch;
      LBRStateMap *smap;
      DICFG *cfg;

      /* TODO: free memory. */

      as = (BPatch_addressSpace*) M;
      is_bin_edit = dynamic_cast<BPatch_binaryEdit*>(as) != NULL;

      assert(is_bin_edit);
      if(is_bin_edit) {
        printf("Running (binary edit)...\n");

        path = strdup(get_binary_path(as).c_str());
        if(!path || strlen(path) < 1) {
          if(!path) path = "NULL";
          fprintf(stderr, "Invalid binary path (%s)\n", path);
          return false;
        }
        bin = basename((char*)path);
        printf("Analyzing module %s (%s)\n", path, bin);

        output = strdup(std::string(bin).append(optOutput.getValue()).c_str());
        stats  = strdup(std::string(bin).append(optStats.getValue()).c_str());

	if(optOutput.getValue().size() > 0) {
          printf("Writing output to %s\n", output);
        } else {
          output = NULL;
          fprintf(stderr, "Warning: no output file selected\n");
        }

	if(optStats.getValue().size() > 0) {
          printf("Writing statistics to %s\n", stats);
        } else {
          stats = NULL;
          if(optCFGStats) {
            fprintf(stderr, "No statistics file selected\n");
            return false;
          }
          fprintf(stderr, "Warning: no statistics file selected\n");
        }

        if(optWindowSize.getValue() < 2 || optWindowSize.getValue() > 16) {
          fprintf(stderr, "Invalid window size %u (valid range [2,16])\n", optWindowSize.getValue());
          return false;
        } else {
          printf("Using window size %u\n", optWindowSize.getValue());
        }
        printf("Using LBR size %u\n", LBR_SIZE);

        if(!set_environment(path)) {
          return false;
        }

        printf("####### parsing CFG #######\n");
        cfg = dyninst_build_cfg(as);
        if(!cfg) {
          fprintf(stderr, "CFG generation failed\n");
          return false;
        }

        if(optDaemon) {
       	  printf("####### running in daemon mode #######\n");
          if(run_lbr_state_analysis_daemon(cfg) < 0) {
            fprintf(stderr, "Error: daemon failed!\n");
            return false;
          }
        } else {
          printf("####### running state analysis #######\n");
          if(!(smap = run_lbr_state_analysis(cfg, stats))) {
            return false;
          }

          if(output && !optCFGStats) {
            printf("####### writing output to %s #######\n", output);
            if(!write_lbr_state_file(cfg, smap, output)) {
              return false;
            }
          }
        }

        return false;
      } else {
        fprintf(stderr, "Only static runs supported, aborting...\n");
        return false;
      }

      return false;
    }

  private:
    BPatch_addressSpace *as;


    bool set_environment(const char *path)
    {
      setenv("BIN"           , path                              , 0);
      setenv("PATHARMOR_ROOT", PATHARMOR_ROOT                    , 0);
      setenv("ICALL_MAP_TYPE", optICallMapType.getValue().c_str(), 0);
      /*setenv("LBR_LIBSYMS"   , LBR_LIBSYMS                       , 0);*/
      /* XXX: use application-specific libsyms file from local directory */
      setenv("LBR_LIBSYMS"   , "libsyms.rel"                     , 0);

      setenv("LBR_BININFO"   , LBR_BININFO                       , 0);

/*
      if(system(PATHARMOR_ROOT"/init.sh") != 0) {
        fprintf(stderr, "Failed to run %s/init.sh\n",PATHARMOR_ROOT);
        return false;
      }
*/
      return true;
    }


    std::string get_binary_path(BPatch_addressSpace *as)
    {
      std::vector<BPatch_object*> objs;

      as->getImage()->getObjects(objs);
      assert(objs.size() > 0);
      if(objs.size() > 1) {
        fprintf(stderr, "Warning: address space has multiple images, using the first\n");
      }
      return string(objs[0]->pathName());
    }


    int preprocess_cfg(CFG *cfg)
    {
      struct timeval start, end;

      if(!strcmp(optICallMapType.getValue().c_str(), "naive")) {
        dyninst_analyze_address_taken(as, (DICFG*)cfg);
      }

      /* This marks address-taken functions independently of the 
       * specific strategy used to resolve icalls. */
      cfg->mark_at_functions();

      if(!add_dummy_shlib_blocks(cfg)) {
        fprintf(stderr, "Failed to add dummy shlib edges to CFG\n");
        return -1;
      }

      gettimeofday(&start, NULL);
      if(!simplify_cfg(cfg)) {
        fprintf(stderr, "CFG simplification failed\n");
        return -1;
      }
      gettimeofday(&end, NULL);
      printf("CFG simplification completed in %.6f seconds\n", 
             (end.tv_sec + (end.tv_usec / 1000000.0)) - 
             (start.tv_sec + (start.tv_usec / 1000000.0)));

	  getAddressTakenParser(optAddressTakenPolicy.getValue().c_str())->set(as, cfg);

      return 0;
    }

    void dump_lbr(struct lbr_t *lbr)
    {
      int i;

      for(i = 0; i < LBR_SIZE; i++) {
        jit_printf("lbr[%2d], <from: 0x%016jx, to: 0x%016jx>\n", i,
                   lbr->from[(lbr->tos+i+1) % LBR_SIZE],
                   lbr->  to[(lbr->tos+i+1) % LBR_SIZE]);
      }
    }


    int run_lbr_state_analysis_daemon(DICFG *cfg)
    {
      int i, j, ret, is_valid;
      LBRConfig conf;
      struct timeval start, end;
      struct offsets_t offsets, offsets_copy;
      struct lbr_t lbr;
      struct lib_index libidx[MAX_LIBS], libidx_tmp[MAX_LIBS];
      struct plt_got_copy_t plt_got_copy, plt_got_another_copy;
      struct simples_t simples;

      plt_got_another_copy.plts = plt_got_copy.plts  = NULL;
      plt_got_another_copy.gots = plt_got_copy.gots  = NULL;
      simples.exitpoints = NULL;

      conf.set_window_size(optWindowSize.getValue());
      conf.set_icall_map_type(optICallMapType.getValue());

      if(preprocess_cfg(cfg) < 0) {
        return -1;
      }

      fprintf(stderr,"Initializing JIT daemon\n");

      armor_fd   = -1;
      run_daemon =  1;

      signal(SIGINT, sigint_handler);

      armor_fd = open("/dev/armor", O_RDONLY);
      if(armor_fd < 0) {
        perror("Could not open /dev/armor");
        return -1;
      }

      ret = ioctl(armor_fd, ARMOR_IOC_GET_OFFSETS, &offsets);
      if(ret) {
        perror("Could not get library offsets from armor module");
        ret = -1; goto cleanup;
      }

      printf("- Got library offsets from armor module:\n");
      dump_offsets(&offsets);


      printf("Fetching plt_got_copy sruct\n");
      ret = ioctl(armor_fd, ARMOR_IOC_PULL_PLT_COPY, &plt_got_copy);
      if(ret) {
        perror("Could not get plt_got_copy struct");
        ret = -1; goto cleanup;
      }

      plt_got_copy.plts = (uint64_t*)malloc(plt_got_copy.size);
      plt_got_another_copy.plts = (uint64_t*)malloc(plt_got_copy.size);
      if(!plt_got_copy.plts || !plt_got_another_copy.plts) {
        fprintf(stderr, "Out of memory\n");
        ret = -1; goto cleanup;
      }
      plt_got_copy.gots = (uint64_t*)malloc(plt_got_copy.size);
      plt_got_another_copy.gots = (uint64_t*)malloc(plt_got_copy.size);
      if(!plt_got_copy.gots || !plt_got_another_copy.gots) {
        fprintf(stderr, "Out of memory\n");
        ret = -1; goto cleanup;
      }

      printf("Fetching PLTs\n");
      ret = ioctl(armor_fd, ARMOR_IOC_PULL_PLTS, plt_got_copy.plts);
      if(ret) {
        perror("Could not get PLTs");
        ret = -1; goto cleanup;
      }
      printf("Fetching GOTs\n");
      ret = ioctl(armor_fd, ARMOR_IOC_PULL_GOTS, plt_got_copy.gots);
      if(ret) {
        perror("Could not get GOTs");
        ret = -1; goto cleanup;
      }

      printf("Got a PLT table:\n");
      for(i = 0; i < plt_got_copy.items; i++) {
        printf("  %p: %p\n",(void *)plt_got_copy.plts[i], (void *)plt_got_copy.gots[i]);
      }

      printf("Fetching exit points struct\n");
      ret = ioctl(armor_fd, ARMOR_IOC_PULL_EXITS, &simples);
      if (ret) {
          perror("Could not get exit points struct");
          ret = -1; goto cleanup;
      }

      simples.exitpoints = (uint64_t*)malloc(simples.size);
      if (!simples.exitpoints) {
          fprintf(stderr, "Out of memory\n");
          ret = -1; goto cleanup;
      }

      printf("Fetching exit points\n");
      ret = ioctl(armor_fd, ARMOR_IOC_PULL_EXITS_DATA, simples.exitpoints);
      if (ret) {
          perror("Could not get exit points\n");
          ret = -1; goto cleanup;
      }

      printf("Got exitpoints:\n");
      for (i = 0; i < simples.items; i++) {
          printf("  %p\n", (void *) simples.exitpoints[i]);
      }

      fprintf(stderr,"\n\n***** STARTING JIT DAEMON *****\n\n");

      while(run_daemon) {
        jit_printf("Waiting for LBR to analyze\n");
        ret = ioctl(armor_fd, ARMOR_IOC_GET_JIT_WORK, &lbr);
        if(ret) {
          perror("Could not get LBR from armor module");
          fprintf(stderr,"Did you compile LKM with -DARMOR_JIT?\n");
          ret = -1; goto cleanup;
        }

      printf("Fetching another copy of PLTs\n");
      ret = ioctl(armor_fd, ARMOR_IOC_PULL_PLTS, plt_got_another_copy.plts);
      if(ret) {
        perror("Could not get PLTs");
        ret = -1; goto cleanup;
      }
      printf("Fetching another copy of GOTs\n");
      ret = ioctl(armor_fd, ARMOR_IOC_PULL_GOTS, plt_got_another_copy.gots);
      if(ret) {
        perror("Could not get GOTs");
        ret = -1; goto cleanup;
      }

	if(memcmp(plt_got_another_copy.plts, plt_got_copy.plts, plt_got_copy.size)) {
          fprintf(stderr,"PLTs changed! The executable re-started, right? Please restart me too. After that, start a new executable.\n");
          ret = -1; goto cleanup;
        }

	if(memcmp(plt_got_another_copy.gots, plt_got_copy.gots, plt_got_copy.size)) {
          fprintf(stderr,"GOTs changed! The executable re-started, right? Please restart me too. After that, start a new executable.\n");
          ret = -1; goto cleanup;
        }

        jit_printf("Analyzing LBR\n");
        is_valid = jit_validate_lbr(cfg, &lbr, &offsets, libidx, &plt_got_copy, &simples);

        jit_printf("State is %s, reporting to armor module\n", is_valid ? "valid" : "invalid");

        ret = ioctl(armor_fd, ARMOR_IOC_PUT_JIT_WORK, is_valid);
        if(ret) {
          perror("Could not report to armor module");
          ret = -1; goto cleanup;
        }
      }

      ret = 0;
cleanup:
      printf("JIT daemon shutting down\n");
      if(plt_got_copy.plts)  free(plt_got_copy.plts);
      if(plt_got_copy.gots)  free(plt_got_copy.gots);
      if(simples.exitpoints) free(simples.exitpoints);
      if(ret != 0) ret = -1;
      if(armor_fd != -1) close(armor_fd);

      return ret;
    }


    LBRStateMap *run_lbr_state_analysis(CFG *cfg, const char *stats)
    {
      LBRStateMap *smap;
      LBRConfig conf;
      struct timeval start, end;

      conf.set_window_size(optWindowSize.getValue());
      conf.set_icall_map_type(optICallMapType.getValue());

      if(preprocess_cfg(cfg) < 0) {
        return NULL;
      }

      if(!optCFGStats) {
        gettimeofday(&start, NULL);
        smap = get_lbr_states(cfg, &conf);
        if(!smap) {
          fprintf(stderr, "Failed to parse state map from CFG\n");
          return NULL;
        }
        gettimeofday(&end, NULL);
        printf("LBR state search completed in %.6f seconds\n", 
               (end.tv_sec + (end.tv_usec / 1000000.0)) - 
               (start.tv_sec + (start.tv_usec / 1000000.0)));
      }

      if(optStats.getValue().size() > 0) {
        printf("Writing statistics to %s\n", stats);
        if(dump_search_stats(stats, cfg, &conf) < 0) {
          return NULL;
        }
      }

      return smap;
    }


    bool write_lbr_state_file(CFG *cfg, LBRStateMap *smap, const char *output)
    {
      bool ret;
      struct lbr_paths *kstates;

      kstates = NULL;
      ret     = false;

      kstates = (struct lbr_paths*)malloc(sizeof(*kstates));
      if(!kstates) {
        fprintf(stderr, "Out of memory\n");
        goto done;
      }
          
      if(generate_lbr_kstates(cfg, smap, kstates) < 0) {
        fprintf(stderr, "Failed to convert state map to kstates\n");
        goto done;
      }

      if(write_lbr_kstates(output, kstates) < 0) {
        fprintf(stderr, "Failed to write kstates to %s\n", output);
        goto done;
      }

      printf("Wrote %u state maps to %s\n", kstates->state_maps, output);

      ret = true;
    done:
      if(kstates) {
        lbr_free_paths(kstates);
        free(kstates);
      }
      return ret;
    }

  };

}

char LBR_AnalysisPass::ID = 0;
RegisterPass<LBR_AnalysisPass> MP("lbr_analysis_pass", "LBR analysis pass");

