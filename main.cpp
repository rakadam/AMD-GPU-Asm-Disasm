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
	
	vector<uint32_t> code = 
	{
		0x00ac1f80,
		0x101a2000,
		0xefbeadde,
		0x00000000,
		0x00ac1f80,
		0x101a2020,
		0xdeadbeef,
		0x00000000,
		0x00a01f80,
		0x101a2040,
		0xadde0000,
		0x00000000,
		0x00a41f80,
		0x101a2060,
		0xefbe0000,
		0x00000000
	};
	
	code = gpu_asm::byte_mirror(code);
	
	
	gpu_disassembler dis(asmdef);
	
	cout << dis.disassemble(code) << endl;
	
	ifstream f2("test.asm");
	
	auto codes = assembler.assemble(std::string(std::istreambuf_iterator<char>(f2), std::istreambuf_iterator<char>()));
}
