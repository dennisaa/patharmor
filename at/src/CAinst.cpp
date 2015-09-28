#include <stdio.h>
#include <stdlib.h>

//dyninst header
#include "BPatch.h"
#include "BPatch_addressSpace.h" 
#include "BPatch_process.h" 
#include "BPatch_binaryEdit.h" 
#include "BPatch_function.h"
#include "BPatch_point.h"
#include "BPatch_flowGraph.h" 
#include "BPatch_object.h"

#include "PatchCommon.h"
#include "PatchMgr.h"
#include "PatchModifier.h"

//dynamorio header
#include "Register.h"
#include "dr_api.h"
#include <dr_defines.h>

//std header
#include <vector>
#include <set>

//Code Armor header
#include "CACustomSnippet.h"
#include "ca_defines.h"
#include "CADecoder.h"
#include "CADecoderDynamoRIO.h"


using namespace std;
using namespace Dyninst;
using namespace Dyninst::PatchAPI;
using namespace Dyninst::InstructionAPI;

BPatch *_bpatch = NULL;
BPatch_addressSpace *_mainApp = NULL;
BPatch_image *_mainImg = NULL;
CADecoder *_decoder;
std::map<uint64_t,uint64_t> _refmap;

bool isFunctionStart(const uint64_t val){
	std::vector<BPatch_function *> functions;
	_mainApp->getImage()->findFunction(val, functions);
	
	for(std::vector<BPatch_function *>::iterator it = functions.begin(); it != functions.end(); ++it) {
		Dyninst::Address start, end;
		if ((*it)->getAddressRange(start, end) && (start == val)) {
			return true;
		}
	}
	return false;
}

void read_raw_ck(char* fname){
	FILE *pfile;
	char* line;
	size_t len=0;
	ssize_t read;
	uint64_t ref_addr,tar_addr;
	char buf[256];
	
	pfile = fopen(fname,"r");
	while ((read = getline(&line,&len,pfile))!=-1){
		sscanf(line,"%lx:%lx,%s\n",&ref_addr,&tar_addr,buf);
		//PLT here means, statically, in data section, pointer point to PLT are found.
		//In this case, dyninst sometimes do not conclude that a plt entry is a function
		//So add it no matter what
		if (strcmp(buf,".plt")==0)
			printf("<PLT>%lx:%lx\n",ref_addr,tar_addr);
		
		//Here, the relation plt section will be fill into the got table, and its value
		//is dynamic deceide, so we check if the got offset is dereference later.
		//However we can not ask the value is a function entry since it is static read the binary
		if (strcmp(buf,".rela.plt")==0)
			_refmap[ref_addr] = tar_addr;
		
		//For the rest case, where pointer stored into data/rodata section
		if (isFunctionStart(tar_addr)){
			_refmap[ref_addr] = tar_addr;
			if ((strcmp(buf,".data")==0)||(strcmp(buf,".rodata")==0))
				printf("<DATA>%lx:%lx\n",ref_addr,tar_addr);
		}
		free(line);
		line = 0;
	}
}

bool isRawCK(const uint64_t val){
	for (std::map<uint64_t,uint64_t>::iterator it=_refmap.begin(); it!=_refmap.end(); ++it){
		if (val == it->first)
			return true;
	}
	return false;
}

void dumpRawCK(){
	for (std::map<uint64_t,uint64_t>::iterator it=_refmap.begin(); it!=_refmap.end(); ++it){
		printf("<RAW>%lx:%lx\n",it->first,it->second);
	}
}


void instrumentBasicBlock(BPatch_function * function, BPatch_basicBlock *block)
{
    Instruction::Ptr iptr;
    void *addr;
	uint64_t deref_addr;

//	printf("\t==BB %lx\n",block->getStartAddress());
    // iterate backwards (PatchAPI restriction)
    PatchBlock::Insns insns;
    PatchAPI::convert(block)->getInsns(insns);
    PatchBlock::Insns::reverse_iterator j;
    for (j = insns.rbegin(); j != insns.rend(); j++) {

        // get instruction bytes
        addr = (void*)((*j).first);
        iptr = (*j).second;

		_decoder->decode((uint64_t)addr,iptr);
		deref_addr = _decoder->get_src_abs_addr();
	//	printf("\t\t~~Ins %lx : %lx\n",(uint64_t)addr,deref_addr);
		if (deref_addr == 0)
			continue;
		
		if (isFunctionStart(deref_addr))
			printf("<AT>%lx:%lx\n",(uint64_t)addr,deref_addr);
		else if (isRawCK(deref_addr)){
	//		if ((uint64_t)addr!=_refmap[deref_addr])
				printf("<CK>%lx:%lx\n",(uint64_t)addr,_refmap[deref_addr]);
		}
		else{
		}
    }
}


void instrumentFunction(BPatch_function *function)
{
    std::set<BPatch_basicBlock*> blocks;
    std::set<BPatch_basicBlock*>::reverse_iterator b;
	
    BPatch_flowGraph *cfg = function->getCFG();
    cfg->getAllBasicBlocks(blocks);
    for (b = blocks.rbegin(); b != blocks.rend(); b++) {
        instrumentBasicBlock(function, *b);
    }
}


void instrumentModule(BPatch_module *module)
{
	char funcname[BUFFER_STRING_LEN];
	std::vector<BPatch_function *>* functions;
	functions = module->getProcedures(true);
	Dyninst::Address start, end;

	for (unsigned i = 0; i < functions->size(); i++) {
		BPatch_function *function = functions->at(i);
		function->getName(funcname, BUFFER_STRING_LEN);
		function->getAddressRange(start,end);
//		printf("--Function %lx = %s\n",start,funcname);
		instrumentFunction(function);
	}
}

BPatch_function* getMutateeFunction(const char *name) {                                   
	BPatch_Vector<BPatch_function *> funcs;
	_mainImg->findFunction(name, funcs, true, true, true);
	if (funcs.size() != 1)
		return NULL;
	return funcs.at(0); 
}                                                                                         

BPatch_function* getAnalysisFunction(const char *name) {
	return getMutateeFunction(name);                                                      
} 

void instrumentApplication(string target)
{
	std::vector<BPatch_object *> objects;
	std::vector<BPatch_object *>::iterator obj;
	std::vector<BPatch_module *> modules;
	std::vector<BPatch_module *>::iterator m;
	std::string real_name;

	real_name = target.substr(target.rfind('/'));
	
	modules.clear();
	_mainImg->getObjects(objects);
	for (obj = objects.begin(); obj != objects.end(); ++obj){
		if (real_name.find((*obj)->name())!=std::string::npos)
			(*obj)->modules(modules);
	}

	if (modules.size() <= 0){
		printf("Not found any related object : %s\n",real_name.c_str());
		return;
	}

	for (m = modules.begin(); m!=modules.end(); m++){
		instrumentModule(*m);
	}
}


int main(int argc, char** argv)
{
	BPatch_addressSpace *app;
	std::string binary;
	char* raw_data_file = NULL;
	int is_bin_edit = !getenv("RT_EDIT");

	if (argc < 2) {
		fprintf(stderr, "Usage: %s proc_filename [args]\n",argv[0]);
			return 1;
	}
	if (argc == 3){
		raw_data_file = argv[2];
	}

	binary = argv[1];

	_bpatch = new BPatch;
	if (is_bin_edit) {
		app = _bpatch->openBinary(binary.c_str(), true);
	}
	else{
		app = _bpatch->processCreate(binary.c_str(), (const char**)(argv+1));
	}
	assert(app);
	_mainApp = app;
	_mainImg = _mainApp->getImage();
	_decoder = new CADecoderDynamoRIO();
	
	if (raw_data_file)
		read_raw_ck(raw_data_file);
		#ifdef OVER_CONSERVATIVE
			dumpRawCK();
		#endif

	instrumentApplication(binary);
	
	return(EXIT_SUCCESS);
}
