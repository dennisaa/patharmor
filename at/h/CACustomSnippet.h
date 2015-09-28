#pragma once
#include "PatchModifier.h"
#include "Snippet.h"
#include <iostream>
#include "ca_defines.h"

using namespace std;
using namespace Dyninst;
using namespace Dyninst::PatchAPI;
using namespace Dyninst::InstructionAPI;

class CACustomSnippet : public Snippet
{
	public:
		CACustomSnippet(uint8_t* payload, size_t nbytes){
			set_payload(payload,nbytes);
		};
		virtual bool generate(Point* pt, Buffer &buf){
			Instruction::Ptr iptr = pt->insn();
#if 0
			printf("\nFunction PreDEPIESnippet()\n");
			printf("-current instruction: ");
			cout << iptr->format(0) << "\n";
#endif
			buf.copy(pre,pre_size);
			return true;
		};
		void set_payload(uint8_t* payload, size_t nbytes)
		{
			assert(nbytes<=MAX_RAW_INSN_SIZE);
			for (size_t i=0;i<nbytes;i++){
				pre[i] = payload[i];
			}
			pre_size = nbytes;
		}
	private:
		uint8_t pre[MAX_RAW_INSN_SIZE];
		size_t pre_size;
};
