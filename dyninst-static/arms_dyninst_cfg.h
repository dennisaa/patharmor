#ifndef __DYNINST_CFG_CONSTRUCTION__
#define __DYNINST_CFG_CONSTRUCTION__

class ArmsFunctionWithAddress; 

class DICFG : public CFG {

static Expression::Ptr the_pc;  


public:
	DICFG(const char *module_name, SymtabCodeSource *sts, CodeObject *co, BPatch_addressSpace *handle) : CFG(module_name)
	{
		cfg_sts = sts;
		cfg_co = co;
		cfg_handle = handle;
	}  

	// ParseAPI based 
	void insert_functions_and_bbs(const CodeObject::funclist& funcs);
	void insert_edges(const CodeObject::funclist& funcs);

	// BPatch_API based 
	void insert_functions(std::vector<BPatch_function *> *funcs); 
	void insert_plt_entries(Symtab *symtab); 
	void insert_corner_case_functions(void);
	void insert_interprocedural_edges(std::vector<BPatch_function *> *funcs);
	void analyze_unresolved_control_transfers(std::vector<BPatch_function *> *funcs);

	void parse_all(void);
        void retouch(Address entrypoint);

private:
	// ParseAPI based 
	void copy_edge_type(ArmsEdge *arms_edge, ParseAPI::Edge *edge, bool indirect); 

	// BPatch_API based 
	void insert_intraprocedural_function_flow_graph(BPatch_function *fun); 
	void copy_edge_type(ArmsEdge *arms_edge, BPatch_edge *bp_edge, bool intra); 
	void set_entry_and_exit_points_of_function(BPatch_function *fun); 
	void insert_interprocedural_edge(ArmsFunction *arms_fun, BPatch_point *call_point);

	SymtabCodeSource *cfg_sts;
	CodeObject *cfg_co;
	BPatch_addressSpace *cfg_handle;
};

void dyninst_analyze_address_taken(BPatch_addressSpace *handle, DICFG *cfg);
DICFG* dyninst_build_cfg(BPatch_addressSpace *handle); 

#endif 
