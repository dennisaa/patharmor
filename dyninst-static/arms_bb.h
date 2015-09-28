#ifndef __bb__
#define __bb__

class ArmsEdge; 
class ArmsInstruction; 
class ArmsFunction; 
class CFG; 

class ArmsBasicBlock {
	
	static uint64_t global_id; 
	static uint64_t num_dummy_blocks; 
	
public:
	static ArmsBasicBlock *create_dummy_basic_block(ArmsFunction *fun, CFG *cfg);
	static ArmsBasicBlock *create_dummy_basic_block(address_t addr, ArmsFunction *fun, CFG *cfg);
	static ArmsBasicBlock *create_dummy_basic_block(address_t start, address_t end, ArmsFunction *fun, CFG *cfg);

	ArmsBasicBlock(address_t start, address_t end, address_t last, ArmsFunction *fun, CFG *cfg) : 	
		start_address(start), end_address(end), last_insn_address(last), 
		is_entry_block_(false), is_exit_block_(false), is_dummy_(false), cfg_(cfg) 
	{ 
		id_ = ++global_id; 
		if (fun) funcs.push_back(fun); 
	} 

	ArmsBasicBlock(address_t start, address_t end, address_t last, CFG *cfg) : 	
		start_address(start), end_address(end), last_insn_address(last), 
		is_entry_block_(false), is_exit_block_(false), is_dummy_(false), cfg_(cfg) 
	{ 
		id_ = ++global_id; 
	} 

	~ArmsBasicBlock() {} 

	uint64_t id() { return id_; }

	void set_is_entry_block(void);
	void set_is_exit_block(void);
	void set_if_entry_block(bool entry);
	void set_if_exit_block(bool exit);

	bool is_entry_block(void) { return is_entry_block_; }
	bool is_exit_block(void) { return is_exit_block_; }
	bool equals_bb(ArmsBasicBlock *bb) { return id_ == bb->id_; }
	bool equals_bb_by_addr(ArmsBasicBlock *bb) { return start_address == bb->start_address; }

	address_t get_start_address(void) { return start_address; }
	address_t get_end_address(void) { return end_address; }
	address_t get_last_insn_address(void) { return last_insn_address; } 

	void add_incoming_edge(ArmsEdge *edge);
	int foreach_incoming_edge(int (*callback)(ArmsEdge*,void*), void *arg);
	size_t incoming_edge_count();
	ArmsEdge *get_incoming_edge(size_t i);
	void delete_incoming_edge(size_t i);
	void delete_incoming_edge(ArmsEdge *e);

	void add_outgoing_edge(ArmsEdge *edge); 
	size_t outgoing_edge_count(void);
	ArmsEdge *get_outgoing_edge(size_t i); 
	bool has_no_call_ft_outgoing_edge(void); 
	void delete_outgoing_edge(size_t i);
	void delete_outgoing_edge(ArmsEdge *e);

	bool has_outbound_fastpath(ArmsBasicBlock *bb);

	void drop_call_ft_edge(void); 
	void drop_incoming_call_ft_edge(void); 

	void set_is_dummy(void) { is_dummy_ = true; } 
	bool is_dummy(void) { return is_dummy_; } 

	bool is_ft(void); 
	bool outgoing_is_ft(void); 

	CFG* get_cfg(void) { return cfg_; }


	std::vector<ArmsFunction*>& get_containing_functions(void) { return funcs; } 
	void add_containing_function(ArmsFunction *fun) { 
		funcs.push_back(fun); 
	} 
	/* provided there is exactly one */
	ArmsFunction* get_function(void) {
		if (funcs.size() == 1) return funcs[0];	
		return 0; 
	} 
	int foreach_function(void (*callback)(ArmsFunction*, ArmsBasicBlock*), ArmsBasicBlock *arg);

	string to_string(void); 

	/* provided there is exactly one */
	ArmsBasicBlock *get_preceding_bb(void); 
	ArmsBasicBlock *get_following_bb(void); 
	ArmsBasicBlock *get_fallthrough_bb(void);

	bool forward_connected_with(ArmsBasicBlock *bb); 
	void get_forward_connected_bbs(vector<ArmsBasicBlock*>& forward_connected, bool &all_indirect); 
	void get_forward_connected_bbs(vector<address_t>& forward_connected, bool &all_indirect);  
	void print_forward_connected_bbs(vector<address_t>& forward_connected);

	void compare_edges(ArmsBasicBlock *other_bb); 

	size_t count_instr() { return instructions.size(); }
	ArmsInstruction *get_instr(size_t i) { return instructions.at(i); }

	void print_subgraph(int level);
	bool intra_procedural_indirect_jump_targets_only(void);

private: 
	uint64_t id_; /* internal */

	std::vector<ArmsInstruction *> instructions; 

	std::set<ArmsEdge*>	incoming_edges;
	std::set<ArmsEdge*> 	outgoing_edges;

	/* [start_address, end_address) */
	address_t		start_address;			
	address_t 		end_address;			

	/* the address of the last instruction */
	address_t 		last_insn_address; 		

	/* initialized to false */
	bool is_entry_block_;					
	bool is_exit_block_;					

	bool is_dummy_; 

	/* The function that contains this basic block. */
	std::vector<ArmsFunction*> funcs;						
	/* The CFG that contains this basic block. */
	CFG *cfg_;  
			
};


#endif 
