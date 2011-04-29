#include <iostream>
#include <fstream>
#include "gpu_asm.hpp"

using namespace std;


int main()
{
	ifstream f("r800.def");
	
	string text = std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	
	gpu_asm::asm_definition asmdef(text);
	
	gpu_assembler assembler(asmdef);
	
	ifstream f2("test.asm");
	
	auto codes = assembler.assemble(std::string(std::istreambuf_iterator<char>(f2), std::istreambuf_iterator<char>()));
}
