#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include <sys/time.h>

#include <string>
#include <algorithm>
#include <vector>
#include <set>
#include <map>
#include <stack>
#include <queue>
#include <deque>

using namespace std;

#include "defs.h"
#include "env.h"
#include "arms_utils.h"
#include "arms_edge.h"
#include "arms_bb.h"
#include "arms_function.h"
#include "arms_cfg.h"

#include "lbr-state.h"
#include "lbr_analysis.h"

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

#include "arms_dyninst_cfg.h"

#define DEBUG 1 
#if DEBUG
  #define dbprintf(...) do { fprintf(stdout, __VA_ARGS__); } while(0)
#else
  #define dbprintf(...) do { } while(0)
#endif

#define JIT_DEBUG 1
#if JIT_DEBUG
  #define jit_printf(...) do { fprintf(stdout, __VA_ARGS__); } while(0)
#else
  #define jit_printf(...) do {} while(0)
#endif

#define STAT_DEBUG 1
#if STAT_DEBUG
  #define stat_printf(...) do { fprintf(stdout, __VA_ARGS__); } while(0)
#else
  #define stat_printf(...) do {} while(0)
#endif


#define NOERR 0
#if NOERR
  #define errprintf(...) do { } while(0)
#else
  #define errprintf(...) do { fprintf(stderr, __VA_ARGS__); } while(0)
#endif

#define MAX(n, m) ((n) > (m) ? (n) : (m))

#if HASH_SHA
#include <openssl/sha.h>
#define HASH_CTX             SHA_CTX
#define HASH_INIT(c)         SHA1_Init(c)
#define HASH_UPDATE(c, b, l) SHA1_Update(c, b, l)
#define HASH_FINAL(b, c)     SHA1_Final(b, c)
#elif HASH_MD4
#include <openssl/md4.h>
#define HASH_CTX             MD4_CTX
#define HASH_INIT(c)         MD4_Init(c)
#define HASH_UPDATE(c, b, l) MD4_Update(c, b, l)
#define HASH_FINAL(b, c)     MD4_Final(b, c)
#elif HASH_MD5
#include <openssl/md5.h>
#define HASH_CTX             MD5_CTX
#define HASH_INIT(c)         MD5_Init(c)
#define HASH_UPDATE(c, b, l) MD5_Update(c, b, l)
#define HASH_FINAL(b, c)     MD5_Final(b, c)
#endif


CFG *cfg;
LBRConfig *conf;

struct {
  struct {
    double        time;
    unsigned long collapsed_functions;
    unsigned long collapsed_edges;
    unsigned long created_fastpaths;
    unsigned long func_bb_count;
  } function_collapse;
  struct {
    double        time;
    std::vector<std::string> searched_libcalls;
    unsigned long searched_roots;
    unsigned long aborted_reschedules;
    unsigned long lbr_state_count;
    unsigned long direct_edges_traversed;
    unsigned long indirect_edges_traversed;
    unsigned long avoided_cycles;
    unsigned long avoided_non_returning_calls;
    unsigned long avoided_mismatched_calls;
    unsigned long aborted_deep_paths;
    unsigned long deepest_explored_path;
  } state_search;
} stats;


/******************************************************************************
 *                               util functions                               *
 ******************************************************************************/
static std::string
bytes_to_hex(unsigned char *b, size_t len)
{
  size_t i;
  std::string hex;
  char buf[3];

  hex = "";
  for(i = 0; i < len; i++) {
    snprintf(buf, 3, "%02x", b[i]);
    hex.append(buf);
  }

  return hex;
}


static inline bool
edge_is_indirect(ArmsEdge *e)
{
  return (e->is_indirect() || e->is_return());
}

static inline bool
edge_is_direct_call(ArmsEdge *e)
{
    return (e->is_direct_call());
}

static inline bool
edge_is_indirect_call(ArmsEdge *e)
{
    return (e->is_indirect_call());
}

static inline bool
edge_is_indirect_jump(ArmsEdge *e)
{
    return (e->is_indirect_jump());
}


static inline bool
edge_is_return(ArmsEdge *e)
{
  return e->is_return();
}


static inline bool
edge_is_call(ArmsEdge *e)
{
  return ((e->get_type() == arms_direct_call) || (e->get_type() == arms_indirect_call));
}


static inline bool
edge_is_function_exit(ArmsEdge *e)
{
  return (edge_is_indirect(e) || edge_is_return(e) || edge_is_call(e));
}


static inline bool
edge_is_hidden(ArmsEdge *e)
{
  return e->is_hidden();
}


static inline bool
edge_is_fastpath(ArmsEdge *e)
{
  return (e->is_fastpath());
}


static inline const char*
edge_kind(ArmsEdge *e)
{
  static const char *t[] = { 
    "call", "indirect call", "interprocedural jmp", "interprocedural indirect jmp",
    "condition taken", "condition not taken", "jmp", "indirect jmp", "fallthrough",
    "catch", "call fallthrough", "return", "none", "fastpath", "unknown"
  };

  if(e->is_hidden()) {
    return "hidden";
  } else {
    return t[e->get_type()];
  }
}

/******************************************************************************
 *                                   LBRHash                                  *
 ******************************************************************************/
LBRHash::LBRHash() : _hops(0)
{
  /*memset(_bytes, 0, LBR_HASH_LEN);*/
}


void
LBRHash::init(LBRHash *h)
{
  unsigned char hb[LBR_HASH_LEN];

  if(h->_hops > 0) h->bytes(_bytes);
  _hops = h->_hops;
}


void
LBRHash::init(unsigned char *b, size_t h)
{
  set_bytes(b);
  _hops = h;
}


void
LBRHash::bytes(unsigned char *b)
{
  memcpy(b, _bytes, LBR_HASH_LEN);
}


void
LBRHash::get_bytes(unsigned char *b)
{
  bytes(b);
}


void
LBRHash::set_bytes(unsigned char *b)
{
  memcpy(_bytes, b, LBR_HASH_LEN);
}


int
LBRHash::compare(LBRHash *h)
{
  int i;

  for(i = 0; i < LBR_HASH_LEN; i++) {
    if(_bytes[i] < h->_bytes[i]) {
      return -1;
    } else if(_bytes[i] > h->_bytes[i]) {
      return 1;
    }
  }

  return 0;
}


std::string
LBRHash::hex()
{
  return bytes_to_hex(_bytes, LBR_HASH_LEN);
}


size_t
LBRHash::hops()
{
  return _hops;
}


size_t
LBRHash::get_hops()
{
  return hops();
}


void
LBRHash::set_hops(size_t h)
{
  _hops = h;
}

/******************************************************************************
 *                                  LBRState                                  *
 ******************************************************************************/
LBRState::LBRState() : _len(0), _hash()
{
}


LBRState::LBRState(ArmsEdge **e, size_t n) : _hash()
{
  set_edges(e, n);
}


void
LBRState::append_edge(ArmsEdge *e)
{
  if(_len < LBR_SIZE) {
    _edge[_len++] = e;
  }
}


ArmsEdge*
LBRState::edge(size_t i)
{
  /*return (i < _len) ? _edge[i] : NULL;*/
  return _edge[i];
}


unsigned int
LBRState::edge_id(size_t i)
{
  /*return (i < _len) ? _edge[i]->id() : 0;*/
  return _edge[i]->id();
}


address_t
LBRState::source(size_t i)
{
  /*return (i < _len) ? _edge[i]->source()->get_last_insn_address() : 0;*/
  return _edge[i]->source()->get_last_insn_address();
}


address_t
LBRState::target(size_t i)
{
  /*return (i < _len) ? _edge[i]->target()->get_start_address() : 0;*/
  return _edge[i]->target()->get_start_address();
}


void
LBRState::set_edges(ArmsEdge **e, size_t n)
{
  size_t i;

  for(i = 0; i < n; i++) {
    _edge[i] = e[i];
  }
  _len = n;
}


size_t
LBRState::len()
{
  return _len;
}


LBRHash*
LBRState::hash()
{
  size_t i;
  address_t edge_src, edge_target;
  unsigned char hb[LBR_HASH_LEN];
  HASH_CTX ctx;

  if(_len == 0) {
    return NULL;
  }

  if(!HASH_INIT(&ctx)) {
    return NULL;
  }

  for(i = 0; i < LBR_SIZE; i++) {
    if(i < _len) {
      edge_src = _edge[i]->source()->get_last_insn_address();
      edge_target = _edge[i]->target()->get_start_address();
    } else {
      edge_src = 0;
      edge_target = 0;
    }

    if(!HASH_UPDATE(&ctx, &edge_src, sizeof(edge_src))) {
      return NULL;
    }
    if(!HASH_UPDATE(&ctx, &edge_target, sizeof(edge_target))) {
      return NULL;
    }
  }

  if(!HASH_FINAL(hb, &ctx)) {
    return NULL;
  }

  _hash.init(hb, _len);

  return &_hash;
}


void
LBRState::dump()
{
  size_t i;

  if(_len == 0) {
    return;
  }

  printf("[%s/%zu] ", hash()->hex().c_str(), hash()->hops());
  for(i = 0; i < _len; i++) {
    printf("0x%jx -> 0x%jx", 
           _edge[i]->source()->get_last_insn_address(),
           _edge[i]->target()->get_start_address());
    if(i != _len-1) {
      printf(" | ");
    }
  }
  printf("\n");
}


bool
LBRState::equals(LBRState *s)
{
  size_t i;

  if(_len != s->_len) {
    return false;
  }

  /* Compare backwards, as the last stored edges are most
   * likely to differ. */
  for(i = _len; i > 0; i--) {
    if(_edge[i-1] != s->_edge[i-1]) {
      return false;
    }
  }

  return true;
}


LBRState*
LBRState::copy()
{
  LBRState *s;

  s = new LBRState();
  memcpy(s->_edge, _edge, sizeof(*_edge)*_len);
  s->_len = _len;
  s->_hash.init(&_hash);

  return s;
}

/******************************************************************************
 *                                 LBRStateSet                                *
 ******************************************************************************/
LBRStateSet::LBRStateSet(ArmsFunction *func) : _func(func), _state(), _states_by_last_edge()
{
}


LBRStateSet::~LBRStateSet()
{
  size_t i;

  for(i = 0; i < _state.size(); i++) {
    if(_state[i]) delete_state(i);
    _state[i] = NULL;
  }
  _state.clear();
}


ArmsFunction*
LBRStateSet::func()
{
  return _func;
}


bool
LBRStateSet::append_state(LBRState *s)
{
  LBRState *t;

  t = s->copy();
  if(!t) {
    return false;
  }
  _state.push_back(t);
  _states_by_last_edge[t->edge_id(t->len()-1)].push_back(t);

  return true;
}


void
LBRStateSet::delete_state(size_t i)
{
  size_t j;
  std::vector<LBRState*> *v;

  if(i > _state.size()) {
    return;
  }

  v = &_states_by_last_edge[_state[i]->edge_id(_state[i]->len()-1)];
  for(j = 0; j < v->size(); j++) {
    if((*v)[j] == _state[i]) {
      if(v->size() > 1) (*v)[j] = (*v)[v->size()-1];
      v->pop_back();
      break;
    }
  }

  delete _state[i];
  if(_state.size() > 1) _state[i] = _state[_state.size()-1];
  _state.pop_back();
}


bool
LBRStateSet::append_states(LBRStateSet *s)
{
  size_t i;

  for(i = 0; i < s->_state.size(); i++) {
    if(append_state(s->_state[i]) < 0) {
      return false;
    }
  }

  return true;
}


LBRState*
LBRStateSet::state(size_t i)
{
  /*return (i < _state.size()) ? _state[i] : NULL;*/
  return _state[i];
}


size_t
LBRStateSet::len()
{
  return _state.size();
}


LBRState*
LBRStateSet::find_state(LBRState *s)
{
  size_t i;
  std::vector<LBRState*> *candidates;

  candidates = &_states_by_last_edge[s->edge_id(s->len()-1)];

  for(i = 0; i < candidates->size(); i++) {
    if(candidates->at(i)->equals(s)) {
      return candidates->at(i);
    }
  }

  return NULL;
}


LBRState*
LBRStateSet::find_state_by_hash(LBRHash *h)
{
  size_t i;

  for(i = 0; i < _state.size(); i++) {
    if(_state[i]->hash()->compare(h) == 0) {
      return _state[i];
    }
  }

  return NULL;
}


void
LBRStateSet::sort_by_hash()
{
  size_t i, j;
  LBRState *sl, *sr;

  for(i = 0; i < _state.size(); i++) {
    for(j = _state.size()-1; j > i; j--) {
      sl = _state[j-1];
      sr = _state[j];
      if(sl->hash()->compare(sr->hash()) > 0) {
        _state[j-1] = sr;
        _state[j] = sl;
      }
    }
  }
}


void
LBRStateSet::dump()
{
  size_t i;

  for(i = 0; i < _state.size(); i++) {
    _state[i]->dump();
  }
}


LBRStateSet*
LBRStateSet::copy()
{
  size_t i;
  LBRStateSet *s;

  s = new LBRStateSet(_func);
  for(i = 0; i < _state.size(); i++) {
    if(s->append_state(_state[i]) < 0) {
      delete s;
      return NULL;
    }
  }

  return s;
}

/******************************************************************************
 *                                 LBRStateMap                                *
 ******************************************************************************/
LBRStateMap::LBRStateMap() : _state_set()
{
}


LBRStateMap::~LBRStateMap()
{
  size_t i;

  for(i = 0; i < _state_set.size(); i++) {
    if(_state_set[i]) delete _state_set[i];
  }
  _state_set.clear();
}


bool
LBRStateMap::append_state_set(LBRStateSet *s)
{
  LBRStateSet *t;

  t = s->copy();
  if(!t) {
    return false;
  }
  _state_set.push_back(t);

  return true;
}


size_t
LBRStateMap::len()
{
  return _state_set.size();
}


LBRStateSet*
LBRStateMap::state_set(size_t i)
{
  /*return (i < _state_set.size()) ? _state_set[i] : NULL;*/
  return _state_set[i];
}

/******************************************************************************
 *                                  LBRConfig                                 *
 ******************************************************************************/
LBRConfig::LBRConfig() : _blacklist(), _window_size(0), _icall_map_type("none")
{
  _init(true, WINDOW_SIZE);
}


LBRConfig::LBRConfig(bool init_blacklist, size_t window_size) : _blacklist(), _window_size(0), _icall_map_type("none")
{
  _init(init_blacklist, window_size);
}


void
LBRConfig::blacklist_func(std::string s)
{
  _blacklist.insert(s);
}


bool
LBRConfig::is_blacklisted(std::string s)
{
  return _blacklist.count(s) > 0;
}


size_t
LBRConfig::blacklist_len()
{
  return _blacklist.size();
}


std::string
LBRConfig::blacklisted_func(size_t i)
{
  std::set<std::string>::iterator iter;

  assert(i < _blacklist.size());

  iter = _blacklist.begin();
  std::advance(iter, i);

  return (*iter);
}


void
LBRConfig::set_window_size(size_t w)
{
  assert(w > 1 && w <= LBR_SIZE);
  assert(w <= WINDOW_SIZE);
  _window_size = w;
}


size_t
LBRConfig::get_window_size()
{
  return _window_size;
}


void
LBRConfig::set_icall_map_type(std::string t)
{
  _icall_map_type = t;
}


std::string
LBRConfig::get_icall_map_type()
{
  return _icall_map_type;
}


void
LBRConfig::_init(bool init_blacklist, size_t window_size)
{
  if(init_blacklist) {
    _blacklist.insert("execve");
    _blacklist.insert("execl");
    _blacklist.insert("execlp");
    _blacklist.insert("execle");
    _blacklist.insert("execv");
    _blacklist.insert("execvp");
    _blacklist.insert("execvpe");
    _blacklist.insert("fexecve");
    _blacklist.insert("mprotect");
    _blacklist.insert("mmap");
    _blacklist.insert("mmap64");
    _blacklist.insert("system");
    _blacklist.insert("syscall");
    /* these functions are dangerous as they can trigger signal handlers */
    _blacklist.insert("raise");
    _blacklist.insert("kill");
    _blacklist.insert("pthread_kill");
    _blacklist.insert("tkill");
    _blacklist.insert("tgkill");
    /* these functions are dangerous as attackers may try to install AT functions as signal handlers */
    _blacklist.insert("sigaction");
    _blacklist.insert("signal");
  }
  set_window_size(window_size);
}

/******************************************************************************
 *                                  DFSState                                  *
 ******************************************************************************/
struct ShadowBB {
  ShadowBB() : scheduled(false), when_scheduled(0) {}
  bool scheduled;
  unsigned int when_scheduled;
};


struct ShadowEdge {
  ShadowEdge() : visited(0), blacklisted(false), cycle_entry(false), cycle_exit(false) {}
  unsigned int visited;
  bool blacklisted;
  bool cycle_entry;
  bool cycle_exit;
};


struct Cycle {
  Cycle() : analyzed(false), direct(true) {}
  bool analyzed;
  bool direct;
};


class DFSState {
public:
  DFSState()                   : len(0), edge(), state_set(NULL), window_shift(0), search_queue(), shadow_edge(), cycle() {}
  DFSState(ArmsFunction *func) : len(0), edge(), state_set(func), window_shift(0), search_queue(), shadow_edge(), cycle() {}
  ~DFSState() {}

  int store_lbr_state()
  {
    LBRState s;

    dbprintf("saving LBR state of length %zu\n", len);

    if(len == 0) {
      return 0;
    }

    s.set_edges(lbr_edge, len);
    if(state_set.find_state(&s)) {
      dbprintf("ignoring duplicate LBR state\n");
      return 0;
    }
    if(state_set.append_state(&s) < 0) {
      errprintf("Out of memory\n");
      return -1;
    }

    return 0;
  }

  void mark_node_scheduled(ArmsBasicBlock *bb)
  {
    shadow_bb[bb->id()].scheduled = true;
    shadow_bb[bb->id()].when_scheduled = window_shift;
  }

  bool node_is_scheduled(ArmsBasicBlock *bb)
  {
    if(shadow_bb.count(bb->id()) == 0) {
      return false;
    } else {
      return shadow_bb[bb->id()].scheduled;
    }
  }

  void schedule_node(ArmsBasicBlock *bb)
  {
    if(!node_is_scheduled(bb)) {
      search_queue.push(bb);
      mark_node_scheduled(bb);
      dbprintf("scheduled node %zu @ 0x%jx for state search\n", bb->id(), bb->get_start_address());
      stats.state_search.searched_roots++;
    } else {
      dbprintf("prevented reschedule of node %zu @ 0x%jx\n", bb->id(), bb->get_start_address());
      stats.state_search.aborted_reschedules++;
    }
  }

  unsigned int when_scheduled(ArmsBasicBlock *bb)
  {
    assert(shadow_bb.count(bb->id()) > 0);
    return shadow_bb[bb->id()].when_scheduled;
  }

  ArmsBasicBlock *next_node()
  {
    ArmsBasicBlock *bb;

    bb = NULL;
    if(search_queue.size() > 0) {
      bb = search_queue.front();
      search_queue.pop();
    }

    return bb;
  }

  void blacklist_edge(ArmsEdge *e)
  {
    shadow_edge[e->id()].blacklisted = true;
  }

  void unblacklist_edge(ArmsEdge *e)
  {
    shadow_edge[e->id()].blacklisted = false;
  }

  bool edge_is_blacklisted(ArmsEdge *e)
  {
    if(shadow_edge.count(e->id()) == 0) {
      return false;
    } else {
      return shadow_edge[e->id()].blacklisted;
    }
  }

  void mark_edge_visited(ArmsEdge *e)
  {
    shadow_edge[e->id()].visited++;
  }

  void unmark_edge_visited(ArmsEdge *e)
  {
    shadow_edge[e->id()].visited--;
  }

  unsigned int edge_is_visited(ArmsEdge *e)
  {
    if(shadow_edge.count(e->id()) == 0) {
      return 0;
    } else {
      return shadow_edge[e->id()].visited;
    }
  }

  void mark_cycle_analyzed(uint64_t sig)
  {
    cycle[sig].analyzed = true;
  }

  bool cycle_is_analyzed(uint64_t sig)
  {
    if(cycle.count(sig) == 0) {
      return false;
    } else {
      return cycle[sig].analyzed;
    }
  }

  ArmsEdge *lbr_edge[LBR_SIZE];
  size_t len;
  std::vector<ArmsEdge*> edge;
  LBRStateSet state_set;

  unsigned int window_shift;
  std::queue<ArmsBasicBlock*> search_queue;

  std::map<uint64_t, ShadowBB> shadow_bb;
  std::map<unsigned int, ShadowEdge> shadow_edge;
  std::map<uint64_t, Cycle> cycle;
};

/******************************************************************************
 *                              get_lbr_states()                              *
 ******************************************************************************/
static bool
edge_is_traversable(ArmsEdge *e, DFSState *dfs)
{
  size_t i;
  int level;
  ArmsBasicBlock *bb;
  ArmsEdge *c, *f;

#if CALL_RET_MATCHING
  if(edge_is_call(e)) {
    c = e;
    bb = c->source()->get_fallthrough_bb();
    level = 0;
    for(i = dfs->edge.size(); i > 0; i--) {
      f = dfs->edge[i-1];
      if(edge_is_call(f)) {
        level++;
      } else if(edge_is_return(f)) {
        if(level != 0) {
          level--;
        } else if(!bb) {
          /* Returns may never match a non-returning call. */
          dbprintf("clipping non-returning call 0x%jx -> 0x%jx (mismatch with return edge 0x%jx -> 0x%jx)\n",
                   c->source()->get_last_insn_address(), c->target()->get_start_address(),
                   f->source()->get_last_insn_address(), f->target()->get_start_address());
          stats.state_search.avoided_non_returning_calls++;
          return false;
        } else if(bb->equals_bb(f->target())) {
          return true;
        } else {
          dbprintf("clipping non-traversable call 0x%jx -> 0x%jx (mismatch with return edge 0x%jx -> 0x%jx)\n",
                   c->source()->get_last_insn_address(), c->target()->get_start_address(),
                   f->source()->get_last_insn_address(), f->target()->get_start_address());
          stats.state_search.avoided_mismatched_calls++;
          return false;
        }
      }
    }
  }
#endif

  return true;
}


static void
filter_impossible_lbr_states(LBRStateSet *sset)
{
  /* XXX: more filter heuristics can be added here. */
}


static uint64_t
is_direct_cycle(ArmsEdge *e, DFSState *dfs, unsigned int *cycle_len)
{
  size_t i;
  uint64_t sig, s;
  ArmsEdge *f;

  if(edge_is_indirect(e)) {
    return 0;
  }

  sig = UINT_MAX;
  for(i = dfs->edge.size()-1; i >= 0; i--) {
    f = dfs->edge[i];

    /* Signature generation based on Marsaglia xorshift. */
    s = (uint64_t)f;
    s ^= s >> 12;
    s ^= s << 25;
    s ^= s >> 27;
    s *= 2685821657736338717ULL;
    sig ^= s;

    if(edge_is_indirect(f)) {
      return 0;
    }
    if(f->id() == e->id()) {
      if(cycle_len) {
        (*cycle_len) = dfs->edge.size()-i;
      }
      return sig;
    }
  }

  return 0;
}


static bool
break_direct_cycle(ArmsEdge *e, DFSState *dfs)
{
  size_t i, j, k, n, s, cycle_start, b;
  ArmsEdge *f;
  std::deque<ShadowEdge> shadow;

  b = 0;
  for(i = dfs->edge.size()-1; i >= 0; i--) {
    f = dfs->edge[i];
    shadow.push_front(ShadowEdge());
    shadow[0].cycle_entry = (f->target()->outgoing_edge_count() > 1);
    shadow[0].cycle_exit  = (f->source()->incoming_edge_count() > 1);

    /* Blacklist every edge on the cycle by default; we later remove
     * the blacklist flag for edges that are on a required path. */
    dfs->blacklist_edge(f);
    b++;

    if(f->id() == e->id()) {
      cycle_start = i;
      break;
    }
  }

  s = shadow.size();
  assert(s > 0);

  /* Self-looping direct cycles can always be safely eliminated. */
  if(s == 1) {
    dfs->unblacklist_edge(dfs->edge[cycle_start]);
    return true;
  }

  /* For each entry point, trace paths to exit points. Blacklist edges which
   * are not on any such path (or rather, don't unblacklist them). */
  for(i = 0; i < s; i++) {
    if(!shadow[i].cycle_entry) {
      continue;
    }
    n = i;
    for(j = i; j != ((i+s)-1) % s; j = (j+1) % s) {
      if(!shadow[j].cycle_exit) {
        continue;
      }
      for(k = n; k != (j+1) % s; k = (k+1) % s) {
        f = dfs->edge[cycle_start+k];
        if(dfs->edge_is_blacklisted(f)) {
          dfs->unblacklist_edge(dfs->edge[cycle_start+k]);
          b--;
        }
      }
      n = (j+1) % s;
    }
  }

  assert(b <= s);
  return (b > 0);
}


static int
search_inbound_edges(ArmsEdge *e, void *arg)
{
  uint64_t cycle_sig;
  unsigned int cycle_len;
  size_t i;
  ArmsBasicBlock *bb;
  ArmsEdge *f;
  DFSState *dfs;

  dfs = (DFSState*)arg;
  bb = e->source();

  if(dfs->edge_is_blacklisted(e)) {
    dbprintf("ignoring blacklisted edge 0x%jx -> 0x%jx (%s)\n",
             e->source()->get_last_insn_address(), e->target()->get_start_address(), edge_kind(e));
    return 0;
  } else if((SEARCH_DEPTH > 0) && (dfs->edge.size() >= SEARCH_DEPTH)) {
    dbprintf("exceeded search depth %u, ignoring edge 0x%jx -> 0x%jx (%s)\n",
             SEARCH_DEPTH, e->source()->get_last_insn_address(), e->target()->get_start_address(), edge_kind(e));
    stats.state_search.aborted_deep_paths++;
    return 0;
  }

  dbprintf("searching edge 0x%jx -> 0x%jx (%s)\n", 
           e->source()->get_last_insn_address(), e->target()->get_start_address(), edge_kind(e));

  if(dfs->edge_is_visited(e)) {
    /* Traversing the current edge would lead to a cycle. If this cycle contains
     * only direct edges, we abort the current path. It does not give us any new 
     * options, and if we continue we will never terminate, as direct cycles do
     * not converge dfs->len towards LBR_SIZE. */
    if((cycle_sig = is_direct_cycle(e, dfs, &cycle_len))) {
      dbprintf("ignoring edge 0x%jx -> 0x%jx to avoid cycle 0x%jx of length %u\n",
               e->source()->get_last_insn_address(), e->target()->get_start_address(), 
               cycle_sig, cycle_len);
      stats.state_search.avoided_cycles++;
      return 0;
    }
  }

  if(edge_is_indirect(e)) {
    dfs->lbr_edge[dfs->len++] = e;
    stats.state_search.indirect_edges_traversed++;
  } else {
    stats.state_search.direct_edges_traversed++;
  }

  dfs->mark_edge_visited(e);
  dfs->edge.push_back(e);

  if(dfs->edge.size() > stats.state_search.deepest_explored_path) {
    stats.state_search.deepest_explored_path = dfs->edge.size();
  }

  if(dfs->len == conf->get_window_size()) { 
    /* Schedule next nodes to be searched such that their first searched
     * edge overlaps with the last of the current iteration. Don't schedule
     * nodes which aren't reachable before the LBR is full. */
    if((1 + dfs->window_shift*(conf->get_window_size()-1)) < LBR_SIZE) {
      dfs->schedule_node(e->target());
    }
  }

  if(dfs->len == conf->get_window_size() || bb->incoming_edge_count() == 0) {
    if(dfs->store_lbr_state() < 0) {
      return -1;
    }
  } else {
    for(i = 0; i < bb->incoming_edge_count(); i++) {
      f = bb->get_incoming_edge(i);
      if(edge_is_hidden(f)) {
        continue;
      } else if(!edge_is_traversable(f, dfs)) {
        continue;
      } else if(search_inbound_edges(f, dfs) < 0) {
        return -1;
      }
    }
  }

  dfs->edge.pop_back();
  dfs->unmark_edge_visited(e);

  if(edge_is_indirect(e)) {
    dfs->len--;
  }

  return 0;
}


static int
get_func_lbr_states(ArmsFunction *func, void *arg)
{
  DFSState dfs(func);
  LBRStateMap *smap;
  ArmsBasicBlock *bb;
  size_t i;

  if(!func->is_lib_dummy()) {
    return 0;
  } else if(!conf->is_blacklisted(func->get_name())) {
    dbprintf("skipping libcall %s (not blacklisted)\n", func->get_name().c_str());
    return 0;
  }

  dbprintf("searching inbound edges to func %s @ 0x%jx\n", 
           func->get_name().c_str(), func->get_base_addr());
  stats.state_search.searched_libcalls.push_back(func->get_name());

  for(i = 0; i < func->nentry_points(); i++) {
    dfs.schedule_node(func->get_entry_point(i));
  }

  while((bb = dfs.next_node()) != NULL) {
    dfs.window_shift = dfs.when_scheduled(bb)+1;
    if(bb->foreach_incoming_edge(search_inbound_edges, &dfs) < 0) {
      return -1;
    }
  }

  filter_impossible_lbr_states(&dfs.state_set);

  dbprintf("parsed %zu lbr states for func %s @ 0x%jx\n", 
           dfs.state_set.len(), func->get_name().c_str(), func->get_base_addr());
  stats.state_search.lbr_state_count += dfs.state_set.len();

  smap = (LBRStateMap*)arg;
  if(smap->append_state_set(&dfs.state_set) < 0) {
    errprintf("Out of memory\n");
    return -1;
  }

  return 0;
}


LBRStateMap*
get_lbr_states(CFG *cfg_, LBRConfig *conf_)
{
  size_t i;
  LBRStateMap *smap;
  struct timeval start, end;

  cfg = cfg_;
  conf = conf_;
  assert(cfg);
  assert(conf);

  smap = new LBRStateMap();
  if(!smap) {
    errprintf("Out of memory\n");
    return NULL;
  }

  gettimeofday(&start, NULL);
  if(cfg->foreach_function(get_func_lbr_states, smap) < 0) {
    goto error;
  }
  gettimeofday(&end, NULL);
  stats.state_search.time = (end.tv_sec + (end.tv_usec / 1000000.0)) - 
                            (start.tv_sec + (start.tv_usec / 1000000.0));

  printf("******* %zu LBR state sets *******\n", smap->len());
  for(i = 0; i < smap->len(); i++) {
    printf("******* %zu LBR states for %s *******\n", 
           smap->state_set(i)->len(),
           smap->state_set(i)->func()->get_name().c_str());
    smap->state_set(i)->dump();
  }

  return smap;

error:
  if(smap) {
    delete smap;
  }

  return NULL;
}


LBRStateMap*
get_lbr_states(CFG *cfg_)
{
  LBRConfig conf_;

  return get_lbr_states(cfg_, &conf_);
}

/******************************************************************************
 *                             jit_validate_lbr()                             *
 ******************************************************************************/
#define JIT_ADDR_UNKNOWN 0xffffffffffffffffULL


static int
jit_track_callstack(ArmsEdge *e, DFSState *dfs, bool clear, bool unroll)
{
  ArmsEdge *c;
  ArmsBasicBlock *bb;
  static std::vector<ArmsBasicBlock*> stack;

#if CALL_RET_MATCHING

  if (clear) {
      stack.clear();
      return 0;
  }

  dbprintf("stack size: %lu\n", stack.size());

  if(edge_is_call(e)) {
    if (unroll) {
        if (!stack.empty()) {
            stack.pop_back();
        }
    } else {
        c = e;
        bb = c->source()->get_fallthrough_bb();
        stack.push_back(bb);
    }
  } else if(edge_is_return(e)) {
    if(stack.empty()) {
      /* We must allow the return, as there may be matching calls
       * outside the range of the LBR */
      return 2;
    }
    bb = stack.back();
    stack.pop_back();
    if (!bb) {
	printf("############ Twilight Zone! ############\n");
	assert(0);
    }
    if(!e->target()->equals_bb(bb)) {
      dbprintf("return edge 0x%jx -> 0x%jx: mismatch with call returning to 0x%jx\n",
               e->source()->get_last_insn_address(), e->target()->get_start_address(),
               bb->get_start_address());
      stack.push_back(bb);
      return 0;
    }
  }
#endif

  return 1;
}


static int
jit_path_exists(ArmsEdge **lbr, unsigned int len, ArmsEdge *e, DFSState *dfs)
{
  size_t i, j, n;
  ArmsBasicBlock *bb;
  ArmsEdge *lbr_src, *lbr_dst, *f;
  static int(*constraints[])(ArmsEdge*, DFSState*, bool, bool) = {
    jit_track_callstack
  };
  static int nb_edges[sizeof(constraints)/sizeof(constraints[0])];

  if((len) < 2) {
    dbprintf("   jit_path_exists: We've found a path that reaches the final LBR edge, so we're done.\n");
    return 1;
  }
  
  bb = e->target();
  lbr_src = lbr[0];
  lbr_dst = lbr[1];

  dbprintf("searching edge 0x%jx -> 0x%jx (%s) (out: %lu)\n",
           e->source()->get_last_insn_address(), e->target()->get_start_address(), edge_kind(e), bb->outgoing_edge_count());
  
  dbprintf("lbr_src: 0x%jx -> 0x%jx (%s)\n", 
           lbr_src->source()->get_last_insn_address(), lbr_src->target()->get_start_address(), edge_kind(lbr_src));
  dbprintf("lbr_dst: 0x%jx -> 0x%jx (%s)\n", 
           lbr_dst->source()->get_last_insn_address(), lbr_dst->target()->get_start_address(), edge_kind(lbr_dst));

  if(dfs->edge_is_visited(e) >= LBR_SIZE) {
    dbprintf("ignoring edge 0x%jx -> 0x%jx (too many cycles)\n", 
             e->source()->get_last_insn_address(), e->target()->get_start_address());
    return 0;
  }

  dfs->mark_edge_visited(e);
  dfs->edge.push_back(e);

  for(i = 0; i < bb->outgoing_edge_count(); i++) {
    f = bb->get_outgoing_edge(i);

    if (edge_is_hidden(f)) {
        dbprintf("   ignoring edge 0x%jx -> 0x%jx (hidden)\n", 
             f->source()->get_last_insn_address(), f->target()->get_start_address());
        continue;
    }

    dbprintf("   analyzing next edge 0x%jx -> 0x%jx (%s)\n",
            f->source()->get_last_insn_address(), f->target()->get_start_address(), edge_kind(f));

    n = sizeof(constraints)/sizeof(constraints[0]);
    for(j = 0; j < n; j++) {
      if(!(nb_edges[j] = constraints[j](f, dfs, false, false))) {
        dbprintf("   One of the constraints prohibits traversing this edge.\n");
        break;
      }
    }
    if(j < n) {
        jit_track_callstack(f, dfs, false, true);
        continue;
    }

    if( (edge_is_indirect(f) || edge_is_direct_call(f)) && (f != lbr_dst)) {
      dbprintf("   This indirect edge cannot be on the path, or it would be in the LBR.\n");
    
      /* if we pushed a return address to the call stack, pop it */
      jit_track_callstack(f, dfs, false, true);
    } else if( (edge_is_indirect(f) || edge_is_direct_call(f)) && (f == lbr_dst)) {
      if((len-1) < 2) {
        dbprintf("   We've found a path that reaches the final LBR edge, so we're done.\n");
        return 1;
      }
      else {
          dbprintf("   Looking at next LBR entry. new len: %d\n", len-1);
          if(jit_path_exists(lbr+1, len-1, f, dfs)) {
            return 1;
          }
      }
    } else {
        dbprintf("   entering another recursion\n");
          if(jit_path_exists(lbr, len, f, dfs)) {
            return 1;
        }
    }
  }

  dfs->edge.pop_back();
  dfs->unmark_edge_visited(e);
  
  jit_track_callstack(e, dfs, false, true);

  dbprintf("recursion return 0\n");
  return 0;
}


static int moveblocks(struct lbr_t *lbr, uint64_t target) {
    int i;
    int index, start;

    start = (lbr->tos+0+1) % LBR_ENTRIES;

    for (i = 0; i < LBR_ENTRIES; i++) {
        index = (lbr->tos+i+1) % LBR_ENTRIES;

        if (lbr->to[index] == target) {
            if (start > index) {
                /* we can now no longer use memmove */
                return 0;
            }

            size_t len = (index - start)*sizeof(uint64_t);
            memmove( &lbr->from[start+1], &lbr->from[start+0], len);
            memmove( &lbr->to  [start+1], &lbr->to  [start+0], len);

            lbr->from[start] = 0x0;
            lbr->to  [start] = 0x0;
        }
    }
    return 1;
}


static int remove_lib_enter_edges(struct lbr_t *lbr, uint64_t target) {
    struct lbr_t copy;
    int i;

    /* The to/from arrays in lbr_t are circular which means that we cannot
     * always do memmove. We will try though, maybe we don't have to do jump
     * magic.
     */
    if (moveblocks(lbr, target)) return 1;

    /* We will copy the lbr struct into a new struct so that it is no longer
     * circular. memmove should then always be possible. Afterwards, we copy the
     * struct back into the original.
     */
    copy.tos = LBR_ENTRIES - 1;
    for (i = 0; i < LBR_ENTRIES; i++) {
        copy.from[i] = lbr->from[(lbr->tos+i+1) % LBR_ENTRIES];
        copy.to  [i] = lbr->to  [(lbr->tos+i+1) % LBR_ENTRIES];
    }
    if (!moveblocks(&copy, target)) return 0; /* this should never happen */
    for (i = 0; i < LBR_ENTRIES; i++) {
        lbr->from[(lbr->tos+i+1) % LBR_ENTRIES] = copy.from[i];
        lbr->to  [(lbr->tos+i+1) % LBR_ENTRIES] = copy.to  [i];
    }
    return 1;
}


static ArmsEdge*
jit_add_plt_got_edge(uint64_t a, uint64_t b, struct offsets_t *offsets)
{
  size_t i;
  ArmsBasicBlock *bb, *cc;
  ArmsFunction *plt, *shlib;
  ArmsEdge *edge, *e, *in;

  plt = cfg->find_function(a);
  if(!plt) {
    errprintf("Cannot find PLT entry 0x%016jx in CFG\n", a);
    return NULL;
  } else if(!plt->is_plt()) {
    errprintf("Dummy at 0x%016jx is not a PLT\n", a);
    return NULL;
  }

  shlib = cfg->create_dummy_function(plt->get_name(), b);
  shlib->set_is_lib_dummy();

  edge = cfg->create_and_add_edge(a, b);
  edge->set_type(arms_inter_indirect_jmp);
  edge->set_interprocedural();
  e = edge;

  dbprintf("created dummy jmp edge 0x%jx -> 0x%jx (%s@plt -> %s)\n", 
           edge->source()->get_last_insn_address(), edge->target()->get_start_address(),
           plt->get_name().c_str(), shlib->get_name().c_str());

  /* Create return edges from the dummy shlib block; these can go to any location
   * from where the PLT entry was called. */ 
  bb = cfg->find_bb(plt->get_base_addr());
  for(i = 0; i < bb->incoming_edge_count(); i++) {
    in = bb->get_incoming_edge(i);
    cc = in->source();             /* bb that called the PLT */
   
    /* if cc.last-instruction == jmp: do another step */
    size_t j;  
    for(j = 0; j < cc->outgoing_edge_count(); j++) {
        ArmsEdge *outgoing;
        outgoing = cc->get_outgoing_edge(j);
        if(!edge_is_call(outgoing)) {
            dbprintf("Outgoing edge 0x%jx -> 0x%jx is not a call instruction. Increasing depth.\n",
                                outgoing->source()->get_last_insn_address(), outgoing->target()->get_start_address());

            size_t k;
            ArmsEdge *in2;
            ArmsBasicBlock *dd;
            for (k = 0; k < cc->incoming_edge_count(); k++) {
                in2 = cc->get_incoming_edge(k);
                dd = in2->source();
                dd = dd->get_fallthrough_bb();
                if (!dd) {
                    errprintf("Warning: no return edge created from %s @ 0x%jx (could be a non-returning function)\n",
                                shlib->get_name().c_str(), shlib->get_base_addr());
                    continue;
                }
            
                dbprintf("created dummy ret edge 0x%jx -> 0x%jx (%s -> call site)\n", 
                    edge->source()->get_last_insn_address(), dd->get_start_address(),
                    shlib->get_name().c_str());
                
                edge = cfg->create_and_add_edge(shlib->get_base_addr(), dd->get_start_address());
                edge->set_type(arms_ret);
                edge->set_interprocedural();
            }
        }
    }

    cc = cc->get_fallthrough_bb(); /* bb where shlib will return */
    if(!cc) {
      errprintf("Warning: no return edge created from %s @ 0x%jx (could be a non-returning function)\n",
                shlib->get_name().c_str(), shlib->get_base_addr());
      continue;
    }
    /* XXX: since we do not know the actual return address from shlib, we instead
     * use its relative start address; the kernel module recognizes this. */
    edge = cfg->create_and_add_edge(shlib->get_base_addr(), cc->get_start_address());
    edge->set_type(arms_ret);
    edge->set_interprocedural();

    dbprintf("created dummy ret edge 0x%jx -> 0x%jx (%s -> call site)\n", 
             edge->source()->get_last_insn_address(), edge->target()->get_start_address(),
             shlib->get_name().c_str());
  }

  return e;
}

int cmpfunc(const void *a, const void *b) {
    uint64_t *leftp = (uint64_t *)a;
    uint64_t *rightp = (uint64_t *) b;

    uint64_t left = *leftp;
    uint64_t right = *rightp;
    if (left < right) return -1;
    if (left > right) return 1;
    return 0;
}


static inline uint64_t
jit_normalize_addr(uint64_t addr, struct offsets_t *offsets, struct lib_index *libidx)
{
  register unsigned int i;
  ArmsFunction *f;
  static uint64_t last_lib_call = JIT_ADDR_UNKNOWN;

  if(cfg->addr_in_cfg(addr)) {
    /* Address inside the binary. */
    dbprintf("normalizing local addr 0x%016jx to 0x%016jx\n", addr, addr);
    return addr;

  } else if(addr == offsets->indirect_call_source) {
    /* Special callback address. */
    dbprintf("normalizing indirect call source 0x%016jx to 0x%016x\n", addr, ADDRESS_TAKEN);
    return ADDRESS_TAKEN;

  } else if(addr == offsets->load_from) {
    /* Loader address. */
    dbprintf("normalizing loader addr 0x%016jx to 0x%016x\n", addr, 0);
    return 0;

  } else if(addr == offsets->armor_lib_return) {
    /* Return from library. In the CFG the source of this return
     * is the base address of the library dummy. This should correspond
     * to the most recently called library address in the LBR. */
    dbprintf("normalizing lib return addr 0x%016jx to 0x%016jx\n", addr, last_lib_call);
    return last_lib_call;

  } else if((addr == offsets->pthread_create) || (addr == offsets->pthread_create_return)) {
    /* Call to or return from pthread_create. We look up the lib dummy
     * for pthread_create to find its actual offset. This may take a bit
     * of time, but it should not be a common case. */
    f = cfg->find_lib_dummy_by_name("pthread_create");
    if(!f) {
      errprintf("Cannot find lib dummy for pthread_create!\n");
      return addr;
      return JIT_ADDR_UNKNOWN;
    }
    dbprintf("normalizing pthread addr 0x%016jx to 0x%016jx\n", addr, f->get_base_addr());
    return f->get_base_addr();

  } else {
    /* Call to library. */
    for(i = 0; i < offsets->libs; i++) {
      if((offsets->start[i] < addr) && (addr < offsets->end[i])) {
        dbprintf("normalizing lib addr 0x%016jx to ", addr);
        last_lib_call = addr;
        dbprintf("0x%016jx\n", addr);
#ifdef JIT_FORCE_VALID
        if(!cfg->find_bb(addr)) {
          /* Assume that this is a library return from an unknown address. 
           * This is for testing an optimization which may cause such addresses
           * to be present in the LBR. */
          dbprintf("rewriting non-normalized lib return addr 0x%016jx to 0x%016jx\n", addr, last_lib_call);
          return last_lib_call;
        }
#endif
        return addr;
      }
    }
  }

  dbprintf("cannot normalize addr 0x%016jx\n", addr);

  return JIT_ADDR_UNKNOWN;
}
    
uint64_t t_empties = 0, 
         t_loads = 0, 
         t_calls = 0, 
         t_icalls = 0, 
         t_ijumps = 0, 
         t_loc_rets = 0,
         t_lib_rets = 0, 
         t_unknown = 0,
         t_iindex = 0,
         t_indirects = 0,
         t_lbrs = 0;
  
#if STAT_DEBUG
static void stat_dump_lbr(struct lbr_t *lbr, struct offsets_t *offsets) {
    int i;
    uint64_t a, b;
    ArmsEdge *e;
    string type;

    uint64_t  empties = 0, 
                loads = 0, 
                calls = 0, 
               icalls = 0, 
               ijumps = 0, 
             loc_rets = 0,
             lib_rets = 0, 
              unknown = 0,
               iindex = 0,
            
            indirects = 0;

    for(i = 0; i < LBR_ENTRIES; i++) {
        a = lbr->from[(lbr->tos+i+1) % LBR_SIZE];
        b = lbr->  to[(lbr->tos+i+1) % LBR_SIZE];
   
        if      (a == 0x00 && b == 0x00)         {  empties++;              type = "empty";        }
        else if (a == offsets->load_from)        {    loads++;              type = "ARMOR.load";   }
        else if (a == offsets->armor_lib_return) { lib_rets++; indirects++; type = "lib.return"; }
        else {
            e = cfg->find_edge(a, b);
            if (e == NULL) { unknown++; type = "unknown"; }
            else {
                string kind(edge_kind(e));
                if (edge_is_indirect(e) && !iindex) iindex = i;

                     if (edge_is_direct_call(e))   {    calls++;              type = "call"; }
                else if (edge_is_indirect_call(e)) {   icalls++; indirects++; type = "indirect.call"; }
                else if (edge_is_indirect_jump(e)) {   ijumps++; indirects++; type = "indirect.jump"; }
                else if (edge_is_return(e))        { loc_rets++; indirects++; type = "return"; }
                else                               {  unknown++;              type = kind + " > unknown"; }
            }
        }
        printf("lbr[%2d], <from: %16p, to: %16p>  %s\n", i, (void *)a, (void *)b, type.c_str());

    }

printf("!ind.: %3lu, call: %3lu, icall: %3lu, ijmp: %3lu, locret: %3lu, libret: %3lu, iindex: %3lu\n",
         indirects,   calls,       icalls,       ijumps,      loc_rets,      lib_rets,      iindex);

    t_indirects += indirects;
    t_calls     += calls;
    t_icalls    += icalls;
    t_ijumps    += ijumps;
    t_loc_rets  += loc_rets;
    t_lib_rets  += lib_rets;
    t_iindex    += iindex;
    t_empties   += empties;
    t_unknown   += unknown;
    t_loads     += loads;
    t_lbrs++;

printf("#ind.: %3lu, call: %3lu, icall: %3lu, ijmp: %3lu, locret: %3lu, libret: %3lu, iindex: %3lu, e: %3lu, u: %0lu | l: %2lu, lbrs: %lu\n",
       t_indirects, t_calls,     t_icalls,     t_ijumps,    t_loc_rets,    t_lib_rets,    t_iindex,      t_empties,t_unknown,t_loads,  t_lbrs);
}
#else
static void stat_dump_lbr(struct lbr_t *lbr, struct offsets_t *offsets) { }
#endif

void stat_dump_finals(void) {
    printf("\n");
    printf("__________ TOTALS ____________________________________________\n");
    printf("LBRs validated: %lu\n", t_lbrs);
    printf("indirect branches: %5lu\n", t_indirects);
    printf(" - indirect calls: %5lu\n", t_icalls);
    printf(" - indirect jumps: %5lu\n", t_ijumps);
    printf(" - local returns : %5lu\n", t_loc_rets);
    printf(" - lib returns   : %5lu\n", t_lib_rets);
    printf("direct branches  : %5lu (direct calls only)\n", t_calls);
    printf("empty entries    : %5lu\n", t_empties);
    printf("unknown edges    : %5lu\n", t_unknown);
    printf("\n");
    printf("\n");
    printf("__________ AVERAGES PER LBR __________________________________\n");
    printf("indirect branches: %8.2f\n", t_indirects / (float)t_lbrs);
    printf(" - indirect calls: %8.2f\n", t_icalls / (float)t_lbrs);
    printf(" - indirect jumps: %8.2f\n", t_ijumps / (float)t_lbrs);
    printf(" - local returns : %8.2f\n", t_loc_rets / (float)t_lbrs);
    printf(" - lib returns   : %8.2f\n", t_lib_rets / (float)t_lbrs);
    printf("direct branches  : %8.2f (direct calls only)\n", t_calls / (float)t_lbrs);
    printf("empty entries    : %8.2f\n", t_empties / (float)t_lbrs);
    printf("unknown edges    : %8.2f\n", t_unknown / (float)t_lbrs);
    printf("\n");
    printf("\n");
    printf("__________ KERNEL SUMMARY ____________________________________\n");
    printf("\n");

}
    

static void
jit_dump_lbr(struct lbr_t *lbr)
{
  int i;

  for(i = 0; i < LBR_ENTRIES; i++) {
    jit_printf("lbr[%2d], <from: 0x%016jx, to: 0x%016jx>\n", i,
               lbr->from[(lbr->tos+i+1) % LBR_SIZE],
               lbr->  to[(lbr->tos+i+1) % LBR_SIZE]);
  }
}


int
jit_validate_lbr(CFG *cfg_, struct lbr_t *lbr, struct offsets_t *offsets,
	struct lib_index *libidx, struct plt_got_copy_t *plt_got,
	struct simples_t *simples)
{
  unsigned int i, j, len, in_plt, unsupported;
  ArmsEdge *e, *f;
  ArmsBasicBlock *bb;
  ArmsEdge *edge[LBR_SIZE];
  uint64_t a, b;
  uint64_t orig_a, orig_b;
  DFSState dfs;

  /* Return non-zero iff the path from the LBR exists in the CFG. */

  cfg = cfg_;

  dbprintf("validating LBR state to 0x%016jx\n", lbr->to[lbr->tos]);

  remove_lib_enter_edges(lbr, offsets->armor_lib_enter);
  for (i = 0; i < LBR_SIZE; i++) {
    /* patch simple functions, only for returns. */
    if ( bsearch( &lbr->from[i], simples->exitpoints, simples->items, sizeof(uint64_t), cmpfunc) ) {
      dbprintf("normalizing lbr ret 0x%016jx to 0x%016jx\n", lbr->from[i], offsets->armor_lib_return);
      lbr->from[i] = offsets->armor_lib_return;
    } 
  }


  /* Get normalized edge objects for all LBR edges. */
  len = 0;
  for(i = 0; i < LBR_SIZE; i++) {
    a = lbr->from[(lbr->tos+i+1) % LBR_SIZE];
    b = lbr->  to[(lbr->tos+i+1) % LBR_SIZE];

    orig_a = a;
    orig_b = b;

    if ( ((a == offsets->armor_lib_return) && (i == 0))  ||      // first entry
         ((a == offsets->armor_lib_return) && (lbr->from[(lbr->tos+i) % LBR_SIZE] == 0) && 
                                              (lbr->  to[(lbr->tos+i) % LBR_SIZE] == 0)) ) {
      /* We have no way to determine the correct return source as the preceding
       * call is not in the LBR. */
      dbprintf("cannot determine source for lib return at LBR[%u], skipping\n", i-1);
      continue;
    } else if((a == 0) && (b == 0)) {
      continue;
    }

    a = jit_normalize_addr(a, offsets, libidx);
    b = jit_normalize_addr(b, offsets, libidx);
    if(a == JIT_ADDR_UNKNOWN) {
      if(JIT_DEBUG) {
        jit_dump_lbr(lbr);
        jit_printf("- Edge[%2d] address unknown: %016jx\n\n", i, b);
      }
      goto lbr_invalid;
    }
    if(b == JIT_ADDR_UNKNOWN) {
      if(JIT_DEBUG) {
        jit_dump_lbr(lbr);
        jit_printf("- Edge[%2d] address unknown: %016jx\n\n", i, b);
      }
      goto lbr_invalid;
    }

    e = cfg->find_edge(a, b);
    if(!e && !cfg->find_bb(b) && b < 0x4000000 /* XXX heuristic to match main executable */ ) {
        dbprintf("0x%lx-0x%lx edge not found! and target bb missing! re-analysing target\n", a, b);
        ((DICFG *) cfg)->retouch(b);
        e = cfg->find_edge(a, b);
        dbprintf("done. edge retry: %p\n", e);
    }

    if(!e) {
      ArmsBasicBlock *iblock_src = cfg->find_bb_by_last_insn_address(a);
      ArmsBasicBlock *iblock_dst = cfg->find_bb(b);
           dbprintf("edge not found! testing for intraprocedural indirect jump.\n");
      if (!iblock_src) {
           dbprintf("no: no bb for source found\n");
      } else if (iblock_src->outgoing_edge_count() != 1) {
      // this block should have only one outgoing edge
	   int ie;
           dbprintf("no: %d outgoing edges, expecting 1\n", (int)iblock_src->outgoing_edge_count());
           for(ie = 0; ie < iblock_src->outgoing_edge_count(); ie++) {
		fprintf(stderr, "%d. %lx->%lx (%s->%s) (%s)\n", ie,
			iblock_src->get_outgoing_edge(ie)->source()->get_last_insn_address(),
			iblock_src->get_outgoing_edge(ie)->target()->get_start_address(),
			iblock_src->get_outgoing_edge(ie)->source()->to_string().c_str(),
			iblock_src->get_outgoing_edge(ie)->target()->to_string().c_str(),
			iblock_src->get_outgoing_edge(ie)->to_string().c_str());
           }
	   // iblock_src->print_subgraph(0); 
      } else if (!iblock_src->get_outgoing_edge(0)->target()->is_dummy()) {
      // the outgoing edge should point to a dummy block (speculating here...)
           dbprintf("no: not a dummy edge\n");
      } else if (!iblock_dst) {
	// dst-address in LBR should be the start of a basic block
           dbprintf("no: no target bb\n");
      } else if (iblock_src->get_function() != iblock_dst->get_function()) {
      // jump should not cross function boundaries (this assumes NO shared blocks)
           dbprintf("no: not intra-procedural\n");
      } else {
           // allow this edge by adding it to the cfg
           dbprintf("yes: allowing intra-procedural indirect jump\n");
           e = cfg->create_and_add_edge(a, b);
           e->set_type(arms_indirect_jmp);
           e->set_intraprocedural();
      }
    }

    if(!e) {
      ArmsBasicBlock *iblock_src = cfg->find_bb_by_last_insn_address(a);
           dbprintf("edge not found! testing for intraprocedural indirect jumps.\n");
      if (!iblock_src) {
           dbprintf("no: no bb for source found\n");
      } else if(iblock_src->intra_procedural_indirect_jump_targets_only()) {
	   // All exit paths are intra-procedural indirect jumps.
	   // This edge is also allowed if there are NO outgoing edges.
	   // in that case we guess our analysis is under-estimating
	   // what legitimate edges there are and so we are conservative in
	   // allowing this.
           // Allow this edge by adding it to the cfg
           dbprintf("yes: allowing intra-procedural indirect jump(s) 0x%lx->0x%lx, retouching 0x%lx\n", a, b, b);
           e = cfg->create_and_add_edge(a, b);
           e->set_type(arms_indirect_jmp);
           e->set_intraprocedural();
      } else {
           dbprintf("no: not all intra-proceudral indirects\n");
      }
    }

    if(!e) {
      dbprintf("cannot find edge 0x%016jx -> 0x%016jx in CFG\n", a, b);


      dbprintf("trying the plt->got table...\n");
      in_plt = 0;
      unsupported = 0;
      for(j = 0; j < plt_got->items; j++) {
        if(plt_got->plts[j] == orig_a && plt_got->gots[j] == orig_b) {
          jit_printf("- Edge[%2d] %016jx -> %016jx only in PLT->GOT: ifunc or uninstrumentable?\n", i, orig_a, orig_b);
          jit_printf("           %016jx -> %016jx (normalized)\n", a, b);
          in_plt = 1;
          break;
        }
        if (plt_got->gots[j] & (1ULL << 63) && plt_got->gots[j] == (orig_b | (1ULL << 63)) ) {
            jit_printf("gots[j]: %016jx\n", plt_got->gots[j]);
            jit_printf("orig_b:  %016jx\n", orig_b);
            jit_printf("Edge[%2d] %016jx -> %016jx involves uninstrumentable. not supported\n", i, orig_a, orig_b);
            unsupported = 1;
            break;
        }
      }

      if (!in_plt && !unsupported) {
        /* if outgoing edge of previous basic block is a call to plt stub that was unsupported... */ 
        ArmsBasicBlock *basicblock, *preceding;
        int x;

        basicblock = NULL;
        for (x = 0; basicblock == NULL && x < 16; x++) {
            basicblock = cfg->find_bb_by_last_insn_address(b - x);
        }

        if (basicblock == NULL) {
            /* Could not find preceding basic block. This is probably not related to an uninstrumentable. */
        } else {
            for(j = 0; j < basicblock->outgoing_edge_count(); j++) {
                ArmsEdge *outgoing;
                outgoing = basicblock->get_outgoing_edge(j);
                jit_printf("Previous edge 0x%jx -> 0x%jx\n",
                           outgoing->source()->get_last_insn_address(), outgoing->target()->get_start_address());

                for (x = 0; x < plt_got->items; x++) {
                    if (plt_got->plts[x] & (1ULL << 63) && 
                        plt_got->plts[x] == (outgoing->target()->get_start_address() | (1ULL << 63))) {
                        jit_printf("Edge %016jx -> %016jx involves uninstrumentable. not supported\n", orig_a, orig_b);
                        unsupported = 1;
                        break;
                    }
                }
                if (unsupported) break;
            }
        }
      }

      if (unsupported) {
          jit_printf("-> Could not validate LBR - containing uninstrumentable library function. Assuming valid.\n");
          goto lbr_unsupported;
      }
    
      if(!in_plt) {
            if(JIT_DEBUG) {
              jit_printf("-> Could not validate LBR\n");
              jit_dump_lbr(lbr);
              jit_printf("-> Edge[%2d] %016jx -> %016jx was not found: return from ifunc or uninstrumentable?\n", 
                                                                    i, orig_a, orig_b);
              jit_printf("            %016jx -> %016jx (normalized)\n", a, b);
              jit_printf("\n");
            }
            goto lbr_invalid;
          
      } else {
        e = jit_add_plt_got_edge(a, b, offsets);
        if(!e) {
          errprintf("Failed to create PLT -> GOT edge 0x%016jx -> 0x%016jx\n", a, b);
          goto lbr_invalid;
        }
      }
    }

    edge[len++] = e;
  }

  bool result;

  result = jit_path_exists(edge, len, edge[0], &dfs);

  /* clear the call stack */
  jit_track_callstack(NULL, NULL, true, false);

  if (!result) {
    if(DEBUG) {
      dbprintf("no valid path matching LBR state:\n");
    }
    goto lbr_invalid;
  }

lbr_valid:
  dbprintf("LBR state to 0x%016jx is valid\n", lbr->to[lbr->tos]);
  stat_dump_lbr(lbr, offsets);
  stat_printf("    \\------------------- is valid\n\n");
  return 1;

lbr_invalid:
  stat_dump_lbr(lbr, offsets);
  stat_printf("    \\------------------- is NOT valid\n\n");
  return 0;

lbr_unsupported:
  stat_dump_lbr(lbr, offsets);
  stat_printf("    \\------------------- is not supported\n\n");
  return 2;
}

/******************************************************************************
 *                         add_dummy_shlib_blocks()                           *
 ******************************************************************************/
static int
link_dummy_block_to_at_func(ArmsFunction *at, void *arg)
{
  ArmsBasicBlock *bb;
  ArmsEdge *edge;

  if(!at->addr_taken()) {
    return 0;
  }

  /* XXX: this creates an edge from a dummy address to the function, to mark
   * this as a possible callback path for the kernel loader. The dummy address
   * has no real meaning, and is just needed to be compatible with the ground 
   * truth test cases. */
  bb = ArmsBasicBlock::create_dummy_basic_block(ADDRESS_TAKEN, ADDRESS_TAKEN, NULL, cfg);
  edge = cfg->create_and_add_edge(bb->get_last_insn_address(), at->get_base_addr());
  edge->set_type(arms_indirect_call);
  edge->set_interprocedural();

  dbprintf("created dummy callback edge 0x%jx -> 0x%jx\n",
           edge->source()->get_last_insn_address(), edge->target()->get_start_address());

  return 0;
}


static int
link_dummy_shlib_block_to_plt(ArmsFunction *plt, void *a)
{
  size_t i;
  address_t addr;
  std::map<std::string, address_t> *addrmap;
  ArmsBasicBlock *bb, *cc;
  ArmsFunction *shlib;
  ArmsEdge *edge, *in;

  if(!plt->is_plt()) {
    return 0;
  }

  addrmap = (std::map<std::string, address_t>*)a;
  if(addrmap->find(plt->get_name()) == addrmap->end()) {
    return 0;
  } else {
    addr = addrmap->at(plt->get_name());
  }

  addr |= LIB_FLAG;

  shlib = cfg->create_dummy_function(plt->get_name(), addr);
  shlib->set_is_lib_dummy();

  edge = cfg->create_and_add_edge(plt->get_base_addr(), addr);
  edge->set_type(arms_inter_indirect_jmp);
  edge->set_interprocedural();

  dbprintf("created dummy jmp edge 0x%jx -> 0x%jx (%s@plt -> %s)\n", 
           edge->source()->get_last_insn_address(), edge->target()->get_start_address(),
           plt->get_name().c_str(), shlib->get_name().c_str());

  /* Create return edges from the dummy shlib block; these can go to any location
   * from where the PLT entry was called. */ 
  bb = cfg->find_bb(plt->get_base_addr());
  for(i = 0; i < bb->incoming_edge_count(); i++) {
    in = bb->get_incoming_edge(i);
    cc = in->source();             /* bb that called the PLT */
    cc = cc->get_fallthrough_bb(); /* bb where shlib will return */
    if(!cc) {
      errprintf("Warning: no return edge created from %s @ 0x%jx (could be a non-returning function)\n",
                shlib->get_name().c_str(), shlib->get_base_addr());
      continue;
    }
    /* XXX: since we do not know the actual return address from shlib, we instead
     * use its relative start address; the kernel module recognizes and patches this. */
    edge = cfg->create_and_add_edge(shlib->get_base_addr(), cc->get_start_address());
    edge->set_type(arms_ret);
    edge->set_interprocedural();

    dbprintf("created dummy ret edge 0x%jx -> 0x%jx (%s -> call site)\n", 
             edge->source()->get_last_insn_address(), edge->target()->get_start_address(),
             shlib->get_name().c_str());
  }

  return 0;
}


static int
insert_dummy_loader_block(CFG *cfg_, address_t mainaddr)
{
  ArmsBasicBlock *bb, *cc;
  ArmsEdge *edge;

  bb = ArmsBasicBlock::create_dummy_basic_block(0, 0, NULL, cfg_);
  cc = cfg_->find_bb(mainaddr);
  if(!cc) {
    errprintf("Cannot find bb of main() (0x%lx), possibly incorrect bininfo file?\n", mainaddr);
    return -1;
  }
  edge = cfg_->create_and_add_edge(bb, cc);
  edge->set_type(arms_indirect_call);
  edge->set_interprocedural();

  dbprintf("created dummy loader call edge 0x%jx -> 0x%jx\n",
           edge->source()->get_last_insn_address(), edge->target()->get_start_address());

  return 0;
}


static int
mark_main_function(CFG *cfg_, address_t mainaddr)
{
  ArmsFunction *f;

  f = cfg_->find_function(mainaddr);
  if(!f) {
    errprintf("Cannot find function of main() (0x%lx), possibly incorrect bininfo file?\n", mainaddr);
    return -1;
  }

  f->set_is_main();

  return 0;
}


static int
parse_bininfo_file(const char *fname, std::map<std::string, std::string> *infomap)
{
  unsigned int i;
  FILE *f;
  char buf[1024], *s, *key, *value;

  f = fopen(fname, "r");
  if(!f) {
    errprintf("Failed to open %s\n", fname);
    return -1;
  }

  while(fgets(buf, sizeof(buf), f)) {
    if(buf[0] == '#' || strlen(buf) < 2) {
      continue;
    }
    key = buf;
    s = strchr(buf, (int)'=');
    if(!s) continue;
    s[0] = '\0';
    value = s + 1;
    for(i = 0; i < strlen(value); i++) {
      if(value[i] == '\r' || value[i] == '\n') {
        value[i] = '\0';
        break;
      }
    }
    (*infomap)[std::string(key)] = std::string(value);
    dbprintf("mapped config line <%s, %s>\n", key, value);
  }

  fclose(f);

  return 0;
}


CFG*
add_dummy_shlib_blocks(CFG *cfg_)
{
  const char *bininfo;
  address_t addr;
  std::map<std::string, address_t> addrmap;
  std::map<std::string, std::string> infomap;

  /* XXX: these dummy blocks are needed for compatibility with the kernel module. */

  cfg = cfg_;
  assert(cfg);

  if(cfg->is_library()) {
    errprintf("Warning: will not add dummy edges for library module\n");
    return cfg;
  }

  (void)cfg->foreach_function(link_dummy_block_to_at_func, NULL);

  bininfo = LBR_BININFO;
  if(!bininfo) {
    errprintf("Warning: LBR_BININFO not set, will not add loader edge\n");
  } else {
    fprintf(stderr, "parsing bininfo %s\n", bininfo);
    if(parse_bininfo_file(bininfo, &infomap) < 0) {
      return NULL;
    }
    if(infomap.find("addr.func.main") != infomap.end()) {
      addr = strtoul(infomap["addr.func.main"].c_str(), NULL, 0);
      if(insert_dummy_loader_block(cfg_, addr) < 0) {
        return NULL;
      }
      if(mark_main_function(cfg_, addr) < 0) {
        return NULL;
      }
    }
  }

  return cfg;
}

/******************************************************************************
 *                               simplify_cfg()                               *
 ******************************************************************************/
static int
create_fastpath(ArmsBasicBlock *bb, ArmsBasicBlock *cc)
{
  ArmsEdge *f;

  f = cfg->create_and_add_edge(bb, cc);
  if(!f) {
    errprintf("Out of memory\n");
    return -1;
  }
  f->set_type(arms_fastpath);

  dbprintf("created fastpath 0x%jx -> 0x%jx\n",
           f->source()->get_last_insn_address(), f->target()->get_start_address());

  return 0;
}


static int
collapse_function(ArmsFunction *f, void *arg)
{
  size_t i, j;
  std::vector<ArmsBasicBlock*> blocks;
  std::map<ArmsBasicBlock*, std::vector<ArmsBasicBlock*> > exits;
  std::queue<ArmsBasicBlock*> Q;
  std::map<ArmsBasicBlock*, bool> V;
  ArmsBasicBlock *bb, *cc, *dd;
  ArmsEdge *e;

  if(f->is_plt() || f->is_lib_dummy()) {
    dbprintf("skipping collapse of function %s\n", f->get_name().c_str());
    return 0;
  } else {
    dbprintf("collapsing function %s\n", f->get_name().c_str());
    stats.function_collapse.collapsed_functions++;
  }

  blocks.assign(f->get_basic_blocks()->begin(), f->get_basic_blocks()->end());
  stats.function_collapse.func_bb_count += blocks.size();

  /* For each basic block in the function, do a BFS to find all exit
   * points reachable from it, and create fastpaths to these. */
  while(blocks.size() > 0) {
    bb = blocks.back();
    blocks.pop_back();
    Q.push(bb);
    V.clear();
    V[bb] = true;

    dbprintf("searching for exits reachable from bb 0x%jx\n", bb->get_start_address());
    while(Q.size() > 0) {
      cc = Q.front();
      Q.pop();
      dbprintf("searching bb 0x%jx\n", cc->get_start_address());

      if(cc->outgoing_edge_count() == 0) {
        /* No outgoing edges; assume this is an exit block. */
        if(std::find(exits[bb].begin(), exits[bb].end(), cc) == exits[bb].end()) {
          dbprintf("bb 0x%jx is an exit block (no outgoing edges), creating fastpath\n",
                   cc->get_start_address());
          exits[bb].push_back(cc);
          if(bb != cc) {
            if(create_fastpath(bb, cc) < 0) {
              return -1;
            } else {
              stats.function_collapse.created_fastpaths++;
            }
          }
        }
      }

      for(j = 0; j < cc->outgoing_edge_count(); j++) {
        e = cc->get_outgoing_edge(j);
        if(edge_is_fastpath(e)) {
          continue;
        }

        if(edge_is_function_exit(e)) {
          /* Exit edge; create a fastpath to its source bb. */
          if(std::find(exits[bb].begin(), exits[bb].end(), cc) == exits[bb].end()) {
            dbprintf("bb 0x%jx is an exit block (has exit edge 0x%jx -> 0x%jx), creating fastpath\n",
                     cc->get_start_address(), 
                     e->source()->get_last_insn_address(), e->target()->get_start_address());
            exits[bb].push_back(cc);
            if(bb != cc) {
              if(create_fastpath(bb, cc) < 0) {
                return -1;
              } else {
                stats.function_collapse.created_fastpaths++;
              }
            }
          }
        } else {
          /* Internal edge; hide it and schedule target for search. */
          dbprintf("hiding internal function edge 0x%jx -> 0x%jx\n",
                   e->source()->get_last_insn_address(), e->target()->get_start_address());
          e->set_hidden(true);
          stats.function_collapse.collapsed_edges++;
          dd = e->target();
          if(!V[dd]) {
            Q.push(dd);
            V[dd] = true;
          }
        }
      }
    }
  }

  return 0;
}


CFG*
simplify_cfg(CFG *cfg_)
{
  struct timeval start, end;

  cfg = cfg_;
  assert(cfg);

  gettimeofday(&start, NULL);
  if(cfg->foreach_function(collapse_function, NULL) < 0) {
    return NULL;
  }
  gettimeofday(&end, NULL);
  stats.function_collapse.time = (end.tv_sec + (end.tv_usec / 1000000.0)) - 
                                 (start.tv_sec + (start.tv_usec / 1000000.0));

  return cfg;
}

/******************************************************************************
 *                            dump_search_stats()                             *
 ******************************************************************************/
int
dump_search_stats(const char *fname, CFG *cfg_, LBRConfig *conf_)
{
  size_t i, icall_sites, icall_targets, icall_edges;
  FILE *f;

  cfg  = cfg_;
  conf = conf_;

  f = fopen(fname, "w");
  if(!f) {
    errprintf("Failed to open %s for writing\n", fname);
    return -1;
  }

  if(conf) {
    fprintf(f, "config.lbr_size       %u\n" , LBR_SIZE);
    fprintf(f, "config.window_size    %zu\n", conf->get_window_size());
    fprintf(f, "config.icall_map_type %s\n" , conf->get_icall_map_type().c_str());
    fprintf(f, "config.blacklist      ");
    for(i = 0; i < conf->blacklist_len(); i++) {
      fprintf(f, "%s ", conf->blacklisted_func(i).c_str());
    }
    fprintf(f, "\n");
  }

  fprintf(f, "stats.cfg.basic_blocks         %zu\n", cfg->count_basic_blocks());
  fprintf(f, "stats.cfg.functions            %zu\n", cfg->count_functions());
  fprintf(f, "stats.cfg.edges                %zu\n", cfg->count_edges());
  cfg->count_ats(&icall_sites, &icall_targets, &icall_edges);
  fprintf(f, "stats.cfg.icall_sites          %zu\n", icall_sites);
  fprintf(f, "stats.cfg.icall_targets        %zu\n", icall_targets);
  fprintf(f, "stats.cfg.icall_edges          %zu\n", icall_edges);
  fprintf(f, "stats.cfg.coarse_grained_edges %zu\n", cfg->count_edges_coarse_grained());

  fprintf(f, "stats.function_collapse.time                %.6fs\n", stats.function_collapse.time);
  fprintf(f, "stats.function_collapse.collapsed_functions %lu\n"  , stats.function_collapse.collapsed_functions);
  fprintf(f, "stats.function_collapse.collapsed_edges     %lu\n"  , stats.function_collapse.collapsed_edges);
  fprintf(f, "stats.function_collapse.created_fastpaths   %lu\n"  , stats.function_collapse.created_fastpaths);
  fprintf(f, "stats.function_collapse.func_bb_count       %lu\n"  , stats.function_collapse.func_bb_count);

  fprintf(f, "stats.state_search.time                        %.6fs\n", stats.state_search.time);
  fprintf(f, "stats.state_search.searched_libcalls           ");
  for(i = 0; i < stats.state_search.searched_libcalls.size(); i++) {
    fprintf(f, "%s ", stats.state_search.searched_libcalls[i].c_str());
  }
  fprintf(f, "\n");
  fprintf(f, "stats.state_search.searched_roots              %lu\n", stats.state_search.searched_roots);
  fprintf(f, "stats.state_search.aborted_reschedules         %lu\n", stats.state_search.aborted_reschedules);
  fprintf(f, "stats.state_search.lbr_state_count             %lu\n", stats.state_search.lbr_state_count);
  fprintf(f, "stats.state_search.direct_edges_traversed      %lu\n", stats.state_search.direct_edges_traversed);
  fprintf(f, "stats.state_search.indirect_edges_traversed    %lu\n", stats.state_search.indirect_edges_traversed);
  fprintf(f, "stats.state_search.avoided_cycles              %lu\n", stats.state_search.avoided_cycles);
  fprintf(f, "stats.state_search.avoided_non_returning_calls %lu\n", stats.state_search.avoided_non_returning_calls);
  fprintf(f, "stats.state_search.avoided_mismatched_calls    %lu\n", stats.state_search.avoided_mismatched_calls);
  fprintf(f, "stats.state_search.search_depth_limit          %u\n" , SEARCH_DEPTH);
  fprintf(f, "stats.state_search.aborted_deep_paths          %lu\n", stats.state_search.aborted_deep_paths);
  fprintf(f, "stats.state_search.deepest_explored_path       %lu\n", stats.state_search.deepest_explored_path);

  fclose(f);

  return 0;
}

/******************************************************************************
 *                             cmp_lbr_kstates()                              *
 ******************************************************************************/
static int
sort_kstate_funcs_by_addr(const void *ss, const void *tt)
{
  struct lbr_function *s, *t;

  s = (struct lbr_function*)ss;
  t = (struct lbr_function*)tt;

  return t->fptr - s->fptr;
}


static int
sort_kstate_maps_by_addr(const void *ss, const void *tt)
{
  struct lbr_valid_state_map *s, *t;

  s = (struct lbr_valid_state_map*)ss;
  t = (struct lbr_valid_state_map*)tt;

  return t->to - s->to;
}


static int
sort_kstate_sets_by_hash(const void* ss, const void* tt)
{
  int i;
  struct lbr_valid_state *s, *t;

  s = (struct lbr_valid_state*)ss;
  t = (struct lbr_valid_state*)tt;

  for(i = 0; i < DIGEST_LENGTH; i++) {
    if(s->hash[i] < t->hash[i]) {
      return -1;
    } else if(s->hash[i] > t->hash[i]) {
      return 1;
    }
  }

  return 0;
}


static void
dump_kstate_map(struct lbr_valid_state_map *m)
{
  size_t i, j;
  struct lbr_valid_state *s;

  for(i = 0; i < m->states; i++) {
    s = &m->state[i];

    printf("[%s/%u] ", bytes_to_hex(s->hash, DIGEST_LENGTH).c_str(), WINDOW_SIZE);
    for(j = 0; j < WINDOW_SIZE; j++) {
      printf("0x%jx -> 0x%jx", s->from[j], s->to[j]);
      if(j != WINDOW_SIZE-1) {
        printf(" | ");
      }
    }
    printf("\n");
  }
}


static void
dump_kstate_map_diff(struct lbr_valid_state_map *m, struct lbr_valid_state_map *n)
{
  if(m) {
    printf("******* %u states for map m *******\n", m->states); 
    dump_kstate_map(m);
  } else {
    printf("******* state map m undefined *******\n");
  }
  if(n) {
    printf("******* %u states for map n *******\n", n->states);
    dump_kstate_map(n);
  } else {
    printf("******* state map n undefined *******\n");
  }
}


static int
cmp_lbr_kstate_funcs(struct lbr_paths *s, struct lbr_paths *t)
{
  int ret;
  size_t i, n;

  ret = 0;
  qsort(s->func, s->funcs, sizeof(*s->func), sort_kstate_funcs_by_addr);
  qsort(t->func, t->funcs, sizeof(*t->func), sort_kstate_funcs_by_addr);

  n = MAX(s->funcs, t->funcs);
  for(i = 0; i < n; i++) {
    if(i < s->funcs && i < t->funcs) {
      if(!strcmp(s->func[i].fname, t->func[i].fname)) {
        printf("func %zu matches <%s, %s>\n", i, s->func[i].fname, t->func[i].fname);
      } else {
        printf("func %zu MISMATCH <%s, %s>\n", i, s->func[i].fname, t->func[i].fname);
        ret = -1;
      }
    } else if(i < s->funcs) {
      printf("func %zu MISMATCH <%s, ?>\n", i, s->func[i].fname);
      ret = -1;
    } else {
      printf("func %zu MISMATCH <?, %s>\n", i, t->func[i].fname);
      ret = -1;
    }
  }

  return ret;
}


static int
is_lbr_kstate_map_superset(struct lbr_valid_state_map *m, struct lbr_valid_state_map *n)
{
  size_t i, j;

  for(i = 0; i < n->states; i++) {
    for(j = 0; j < m->states; j++) {
      if(!memcmp(m->state[j].hash, n->state[i].hash, DIGEST_LENGTH)) {
        break;
      }
    }
    if(j == m->states) {
      return 0;
    }
  }

  return 1;
}


static int
is_lbr_kstate_superset(struct lbr_paths *s, struct lbr_paths *t)
{
  size_t i, j;

  for(i = 0; i < t->state_maps; i++) {
    for(j = 0; j < s->state_maps; j++) {
      if(s->state_map[j].to == t->state_map[i].to) {
        if(is_lbr_kstate_map_superset(&s->state_map[j], &t->state_map[i])) {
          break;
        } else {
          return 0;
        }
      }
    }
    if(j == s->state_maps) {
      return 0;
    }
  }

  return 1;
}


static int
cmp_lbr_kstate_maps(struct lbr_valid_state_map *m, struct lbr_valid_state_map *n)
{
  size_t i;

  qsort(m->state, m->states, sizeof(*m->state), sort_kstate_sets_by_hash);
  qsort(n->state, n->states, sizeof(*n->state), sort_kstate_sets_by_hash);

  if(m->states != n->states) {
    return -1;
  }

  for(i = 0; i < m->states; i++) {
    if(memcmp(m->state[i].hash, n->state[i].hash, DIGEST_LENGTH)) {
      return -1;
    }
  }

  return 0;
}


static int
cmp_lbr_kstate_paths(struct lbr_paths *s, struct lbr_paths *t)
{
  int ret;
  size_t i, n;

  ret = 0;
  qsort(s->state_map, s->state_maps, sizeof(*s->state_map), sort_kstate_maps_by_addr);
  qsort(t->state_map, t->state_maps, sizeof(*t->state_map), sort_kstate_maps_by_addr);

  n = MAX(s->state_maps, t->state_maps);
  for(i = 0; i < n; i++) {
    if(i < s->state_maps && i < t->state_maps) {
      if(cmp_lbr_kstate_maps(&s->state_map[i], &t->state_map[i]) < 0) {
        printf("state map 0x%jx MISMATCH\n", s->state_map[i].to);
        dump_kstate_map_diff(&s->state_map[i], &t->state_map[i]);
        ret = -1;
      } else {
        printf("state map 0x%jx matches\n", s->state_map[i].to);
        dump_kstate_map_diff(&s->state_map[i], &t->state_map[i]);
      }
    } else if(i < s->state_maps) {
      printf("state map 0x%jx MISMATCH\n", s->state_map[i].to);
      dump_kstate_map_diff(&s->state_map[i], NULL);
      ret = -1;
    } else {
      printf("state map 0x%jx MISMATCH\n", t->state_map[i].to);
      dump_kstate_map_diff(NULL, &t->state_map[i]);
      ret = -1;
    }
  }

  return ret;
}


int
cmp_lbr_kstates(struct lbr_paths *s, struct lbr_paths *t)
{
  int ret;

  assert(s);
  assert(t);

  ret = 0;

  if(cmp_lbr_kstate_funcs(s, t) < 0) {
    ret = -1;
  }

  if(cmp_lbr_kstate_paths(s, t) < 0) {
    ret = -1;
  }

  if(ret == 0) {
    printf("*~*~*~* ALL STATES MATCHED *~*~*~*\n");
  } else if(is_lbr_kstate_superset(s, t)) {
    printf("*~*~*~* STATE SUPERSET MATCHED *~*~*~*\n");
    ret = 0;
  }

  return ret;
}

/******************************************************************************
 *                           generate_lbr_kstates()                           *
 ******************************************************************************/
static address_t
set_addr_flags(CFG *cfg_, address_t addr)
{
  ArmsFunction *f;

  f = cfg_->find_function(addr);
  if(f && f->is_lib_dummy()) {
    addr |= LIB_FLAG;
  } else if(cfg_->addr_in_cfg(addr)) {
    if(addr != ADDRESS_TAKEN) {
      addr |= TARG_FLAG;
    }
  } else {
    dbprintf("Warning: address 0x%jx not found in cfg range [0x%jx, 0x%jx)\n",
             addr, cfg_->get_start_addr(), cfg_->get_end_addr());
  }

  if(f && f->is_main()) {
    addr |= MAIN_FLAG;
  }

  return addr;
}


int
add_dummy_kstates(struct lbr_paths *kstates)
{
  size_t i, n;
  void *tmp;

  n = kstates->state_maps + 2;

  tmp = realloc(kstates->state_map, sizeof(struct lbr_valid_state_map)*n);
  if(!tmp) {
    errprintf("Out of memory\n");
    return -1;
  }
  kstates->state_map = (struct lbr_valid_state_map*)tmp;

  /* Dummy block allowing 0x00 -> 0x00. */
  i = kstates->state_maps++;
  kstates->state_map[i].to = 0x00;
  kstates->state_map[i].state = (struct lbr_valid_state*)malloc(sizeof(struct lbr_valid_state));
  if(!kstates->state_map[i].state) {
    errprintf("Out of memory\n");
    return -1;
  }
  kstates->state_map[i].states = 1;
  memset(kstates->state_map[i].state[0].from, 0, sizeof(kstates->state_map[i].state[0].from));
  memset(kstates->state_map[i].state[0].to  , 0, sizeof(kstates->state_map[i].state[0].to));

  return 0;
}


static int
set_kstates_map(CFG *cfg_, address_t to, std::vector<LBRState*> *sset, struct lbr_valid_state_map *kstates)
{
  size_t i, j;
  unsigned char hash[LBR_HASH_LEN];

  kstates->to = to;

  kstates->state = (struct lbr_valid_state*)malloc(sizeof(struct lbr_valid_state)*sset->size());
  if(!kstates->state) {
    errprintf("Out of memory\n");
    return -1;
  }

  for(i = 0; i < sset->size(); i++) {
    memset(kstates->state[i].from, 0, sizeof(kstates->state[i].from));
    memset(kstates->state[i].to  , 0, sizeof(kstates->state[i].to));
    for(j = 0; j < sset->at(i)->len(); j++) {
      kstates->state[i].from[j] = set_addr_flags(cfg_, sset->at(i)->source(j));
      kstates->state[i].to[j]   = set_addr_flags(cfg_, sset->at(i)->target(j)); 
    }
    assert(LBR_HASH_LEN == DIGEST_LENGTH);
    /* XXX: disable copying the hash; the states are rehashed later. */
    /*sset->at(i)->hash()->get_bytes(hash);
    memcpy(kstates->state[i].hash, hash, LBR_HASH_LEN);*/
  }
  kstates->states = sset->size();

  return 0;
}


static int
append_at_func(ArmsFunction *f, void *arg)
{
  if(f->addr_taken()) {
    ((std::vector<ArmsFunction*>*)arg)->push_back(f);
  }
  return 0;
}


int
generate_lbr_kstates(CFG *cfg_, LBRStateMap *smap, struct lbr_paths *kstates)
{
  size_t i, j, n;
  address_t to;
  LBRStateSet *sset;
  LBRState *s;
  std::vector<ArmsFunction*> at_funcs;
  std::map<address_t, std::vector<LBRState*> > states_by_addr;
  std::map<std::string, bool> state_lookup;
  std::map<address_t, std::vector<LBRState*> >::iterator it;

  cfg = cfg_;
  assert(cfg);
  assert(smap);
  assert(kstates);

  memset(kstates, 0, sizeof(*kstates));

  n = smap->len();
  kstates->func = (struct lbr_function*)malloc(sizeof(struct lbr_function)*n);
  if(!kstates->func) {
    errprintf("Out of memory\n");
    return -1;
  }

  for(i = 0; i < n; i++) {
    assert(smap->state_set(i)->func()->get_name().size() < MAX_FNAME_LENGTH);
    strcpy(kstates->func[i].fname, smap->state_set(i)->func()->get_name().c_str());
    kstates->func[i].fptr = smap->state_set(i)->func()->get_base_addr();
  }
  kstates->funcs = n;

  cfg->foreach_function(append_at_func, &at_funcs);
  n = at_funcs.size();
  kstates->address_taken = (uint64_t*)malloc(sizeof(uint64_t)*n);
  if(!kstates->address_taken) {
    errprintf("Out of memory\n");
    return -1;
  }
  for(i = 0; i < n; i++) {
    kstates->address_taken[i] = set_addr_flags(cfg, at_funcs[i]->get_base_addr());
  }
  kstates->ats = (uint32_t)n;

  /* XXX: group states by target address, this is how the kernel module expects them. */
  for(i = 0; i < smap->len(); i++) {
    sset = smap->state_set(i);
    for(j = 0; j < sset->len(); j++) {
      s = sset->state(j);
      assert(s->len() > 0);
      to = s->target(0);
      if(!state_lookup[s->hash()->hex()]) {
        states_by_addr[to].push_back(s);
        state_lookup[s->hash()->hex()] = true;
      }
    }
  }
  n = states_by_addr.size();
  if(n > 0) {
    kstates->state_map = (struct lbr_valid_state_map*)malloc(sizeof(struct lbr_valid_state_map)*n);
    if(!kstates->state_map) {
      errprintf("Out of memory\n");
      return -1;
    }

    i = 0;
    for(it = states_by_addr.begin(); it != states_by_addr.end(); it++) {
      if(set_kstates_map(cfg_, it->first, &it->second, &kstates->state_map[i++]) < 0) {
        return -1;
      }
    }
  }
  kstates->state_maps = n;

  /* XXX: add some dummy states expected by the kernel module. */
  if(add_dummy_kstates(kstates) < 0) {
    return -1;
  }

  /* XXX: currently, address type flags are only added to struct lbr_paths,
   * not to LBRStateMap. Thus, we need to recompute the hashes here. */
  hash_paths(kstates);

  dbprintf("generated kstates (%u state sets, %u functions, %u address-taken)\n", 
           kstates->state_maps, kstates->funcs, kstates->ats);

  return 0;
}

/******************************************************************************
 *                             read_lbr_kstates()                             *
 ******************************************************************************/
struct lbr_paths*
read_lbr_kstates(const char *fname)
{
  struct lbr_paths *kstates;

  assert(fname);

  kstates = read_paths(fname);
  if(!kstates) {
    errprintf("Cannot read kstates from file %s\n", fname);
    return NULL;
  }

  hash_paths(kstates);

  dbprintf("read kstates file: %u functions, %u ats, %u state maps\n", 
           kstates->funcs, kstates->ats, kstates->state_maps);

  return kstates;
}

/******************************************************************************
 *                             write_lbr_kstates()                            *
 ******************************************************************************/
int
write_lbr_kstates(const char *fname, struct lbr_paths *kstates)
{
  struct lbr_paths *packed;

  assert(fname);
  assert(kstates);

  packed = lbr_pack_paths(kstates);
  if(!packed) {
    errprintf("Not enough memory to serialize kstates\n");
    return -1;
  }

  if(write_paths(fname, packed) != 0) {
    errprintf("Cannot write kstates to file %s\n", fname);
    return -1;
  }

  free(packed);

  return 0;
}

