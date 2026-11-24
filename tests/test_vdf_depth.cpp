#include "vdf.h"
#include <cassert>
#include <iostream>
static std::vector<uint8_t> Nested(size_t depth){std::vector<uint8_t> bytes;for(size_t i=0;i<depth;++i){bytes.push_back(0);bytes.push_back('n');bytes.push_back(0);}for(size_t i=0;i<depth;++i)bytes.push_back(8);return bytes;}
int main(){Vdf::Node root;auto ordinary=Nested(100);assert(Vdf::Parse(ordinary,root));assert(Vdf::Write(root)==ordinary);
    auto damaged=Nested(100000);if(Vdf::Parse(damaged,root)){std::cerr<<"Unbounded nested VDF accepted\n";return 1;}
    Vdf::Node normal;std::vector<uint8_t> valid{0,'s',0,1,'x',0,'v',0,8};assert(Vdf::Parse(valid,normal));assert(Vdf::Write(normal)==valid);
    std::cout<<"Deep VDF is rejected without stack exhaustion; ordinary raw data round-trips unchanged.\n";
}
