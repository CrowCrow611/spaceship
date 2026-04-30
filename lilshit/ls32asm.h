#pragma once
#include "../ls32/ls32.h"
#include <string>
#include <vector>

namespace LS32 {

struct AsmError {
    int line = -1;
    std::string msg;
};

struct AsmResult {
    bool ok = false;
    std::vector<u8> binary;
    AsmError error;
    u32 instrCount = 0;
};

//assemble a multiline lilshit source string
AsmResult assemble(const std::string& source);

//assemble a file from disk
AsmResult assembleFile(const std::string& path);

} //namespace LS32