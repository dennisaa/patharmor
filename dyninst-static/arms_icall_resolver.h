#ifndef __ICALL_RESOLVER__
#define __ICALL_RESOLVER__

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <stdlib.h>
#include <unistd.h>

#include "env.h"

static inline void split (std::vector<std::string> &tokens, const std::string &text, char sep) {
    int start = 0, end = 0;
    while ((end = text.find(sep, start)) != std::string::npos) {
      tokens.push_back(text.substr(start, end - start));
      start = end + 1;
    }
    tokens.push_back(text.substr(start));
}

static inline int arms_icall_resolver(void *callSiteAddr, std::vector<void*> &targets)
{
    char buf[512];
    std::vector<std::string> tokens;
    sprintf(buf, PATHARMOR_ROOT"/scripts/cem-binmap.sh %lx", (unsigned long) callSiteAddr);
    FILE* stream = popen(buf, "r" );
    if (!stream) {
	perror(buf);
	exit(1);
        return -1;
    }
    std::ostringstream output;

    while( !feof( stream ) && !ferror( stream )) {
        int bytesRead = fread( buf, 1, 512, stream );
        output.write( buf, bytesRead );
    }
    split(tokens, output.str(), ',');
    for (unsigned i=0;i<tokens.size();i++) {
        const char* token = tokens[i].c_str();
        long long tokenll = strtoll(token, NULL, 16);
        targets.push_back((void*) tokenll);
    }

    pclose(stream);

    return 0;
}

#endif

