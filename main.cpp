#include <iostream>
#include <fstream>
#include "gpu_asm.hpp"

using namespace std;


int main()
{
	ifstream f("r800.def");
	
	string text = std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	
	gpu_asm::asm_definition asmdef(text);
	
}
