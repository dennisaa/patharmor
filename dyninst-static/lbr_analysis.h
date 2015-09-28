#ifndef LBR_ANALYSIS_H
#define LBR_ANALYSIS_H

class ArmsFunction;
class CFG;


#define LBR_SIZE 16

#define SEARCH_DEPTH      0
#define CALL_RET_MATCHING 1

#define HASH_SHA 0
#define HASH_MD4 1
#define HASH_MD5 0

#if HASH_SHA
#define LBR_HASH_LEN 20
#elif HASH_MD4
#define LBR_HASH_LEN 16
#elif HASH_MD5
#define LBR_HASH_LEN 16
#endif

class LBRHash {
public:
  LBRHash();

  void init(LBRHash *h);
  void init(unsigned char *b, size_t h);

  void bytes(unsigned char *b);
  void get_bytes(unsigned char *b);
  void set_bytes(unsigned char *b);
  int compare(LBRHash *h);

  std::string hex();

  size_t hops();
  size_t get_hops();
  void set_hops(size_t h);

private:
  unsigned char _bytes[LBR_HASH_LEN];
  size_t _hops;
};


class LBRState {
public:
  LBRState();
  LBRState(ArmsEdge **e, size_t n);

  void append_edge(ArmsEdge *e);
  ArmsEdge *edge(size_t i);
  unsigned int edge_id(size_t i);
  address_t source(size_t i);
  address_t target(size_t i);
  void set_edges(ArmsEdge **e, size_t n);
  size_t len();

  LBRHash *hash();
  void dump();

  bool equals(LBRState *s);
  LBRState *copy();

private:
  ArmsEdge *_edge[LBR_SIZE];
  size_t _len;
  LBRHash _hash;
};


class LBRStateSet {
public:
  LBRStateSet(ArmsFunction *func);
  ~LBRStateSet();

  ArmsFunction *func();

  bool append_state(LBRState *s);
  void delete_state(size_t i);
  bool append_states(LBRStateSet *s);
  LBRState *state(size_t i);
  size_t len();

  LBRState *find_state(LBRState *s);
  LBRState *find_state_by_hash(LBRHash *h);
  void sort_by_hash();
  void dump();

  LBRStateSet *copy();

private:
  ArmsFunction *_func;
  std::vector<LBRState*> _state;
  std::map<unsigned int, std::vector<LBRState*> > _states_by_last_edge;
};


class LBRStateMap {
public:
  LBRStateMap();
  ~LBRStateMap();

  bool append_state_set(LBRStateSet *s);
  size_t len();
  LBRStateSet *state_set(size_t i);

private:
  std::vector<LBRStateSet*> _state_set;
};


class LBRConfig {
public:
  LBRConfig();
  LBRConfig(bool init_blacklist, size_t window_size);

  void blacklist_func(std::string s);
  bool is_blacklisted(std::string s);
  size_t blacklist_len();
  std::string blacklisted_func(size_t i);

  void set_window_size(size_t w);
  size_t get_window_size();

  void set_icall_map_type(std::string t);
  std::string get_icall_map_type();

private:
  std::set<std::string> _blacklist;
  size_t _window_size;
  std::string _icall_map_type;

  void _init(bool init_blacklist, size_t window_size);
};


LBRStateMap *get_lbr_states(CFG *cfg, LBRConfig *conf);
LBRStateMap *get_lbr_states(CFG *cfg);

void stat_dump_finals(void);
int jit_validate_lbr(CFG *cfg, struct lbr_t *lbr, struct offsets_t *offsets,
	struct lib_index *libidx, struct plt_got_copy_t *plt_got,
	struct simples_t *simples);

CFG *add_dummy_shlib_blocks(CFG *cfg);
CFG *simplify_cfg(CFG *cfg);

int dump_search_stats(const char *fname, CFG *cfg, LBRConfig *conf);

int cmp_lbr_kstates(struct lbr_paths *s, struct lbr_paths *t);
int generate_lbr_kstates(CFG *cfg, LBRStateMap *smap, struct lbr_paths *kstates);
struct lbr_paths *read_lbr_kstates(const char *fname);
int write_lbr_kstates(const char *fname, struct lbr_paths *kstates);

#endif /* LBR_ANALYSIS_H */

