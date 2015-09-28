#ifndef __FUNCTION__
#define __FUNCTION__

class ArmsBasicBlock; 
class CFG; 

class ArmsFunction {

static string fun_dummy_name;

public:
	ArmsFunction(address_t addr, string name, CFG *cfg) : 
		base_addr(addr), has_address_taken(false), is_plt_(false), is_lib_dummy_(false), is_main_(false), funname(name), cfg(cfg) {}
	~ArmsFunction() {} 	

	std::set<ArmsFunction*> get_callees(void) { return callees; } 
	std::set<ArmsFunction*> get_callers(void) { return callers; } 

	address_t get_base_addr(void) { return base_addr; }
	void set_addr_taken(void) { has_address_taken = true; }
        bool addr_taken() { return has_address_taken; }

	bool is_plt(void) 	{ return is_plt_; }
	void set_is_plt(void)	{ is_plt_ = true; } 
	bool is_lib_dummy(void)     { return is_lib_dummy_; }
	void set_is_lib_dummy(void) { is_lib_dummy_ = true; }
	void set_is_main(void)      { is_main_ = true; }
	bool is_main(void)          { return is_main_; }

	string get_name() { return funname; }

	void add_bb(ArmsBasicBlock *bb);
	void add_entry_block(address_t bb_addr);
	void add_entry_block(ArmsBasicBlock *bb);
	int foreach_entry_block(int (*callback)(ArmsBasicBlock*,void*), void *arg);
	int foreach_inbound_edge(int (*callback)(ArmsEdge*,void*), void *arg);
	void add_exit_block(address_t bb_addr);
	void add_exit_block(ArmsBasicBlock *bb);

	void add_callee(ArmsFunction *callee) { callees.insert(callee); } 
	void add_caller(ArmsFunction *caller) { callers.insert(caller); } 

	size_t nentry_points() { return entry_points.size(); }
	ArmsBasicBlock *get_entry_point(size_t i);
	size_t nexit_points() { return exit_points.size(); }
	ArmsBasicBlock *get_exit_point(size_t i);

	std::set<ArmsBasicBlock*>* get_basic_blocks(void) { return &basic_blocks; }
	std::set<ArmsBasicBlock*>* get_entry_points(void) { return &entry_points; }
	std::set<ArmsBasicBlock*>* get_exit_points(void)  { return &exit_points; }

	void add_external_call(ArmsBasicBlock *bb) { external_calls.insert(bb); }

	string to_string(void); 

private:
	/* XXX THIS CAN BE USED ONLY AS AN ID!! and not an address */
	address_t base_addr; 

	/* by default, initialized to false */
	bool has_address_taken; 
	bool is_plt_;
	bool is_lib_dummy_;
	bool is_main_;
 	string funname; 

	std::set<ArmsBasicBlock*> basic_blocks;

	std::set<ArmsBasicBlock*> entry_points; 

	/* Basic blocks that contain a ret instruction. */
	std::set<ArmsBasicBlock*> exit_points; 

	/* Basic blocks that contain a call to an external module. */
	std::set<ArmsBasicBlock*> external_calls; 

	/* Callees of this function, i.e., functions called by this one. */
	std::set<ArmsFunction*> callees;

	/* Callers of this function, i.e., functions calling this one. */
	std::set<ArmsFunction*> callers;

	CFG *cfg; 
};

#endif
