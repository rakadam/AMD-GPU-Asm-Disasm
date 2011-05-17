#include <iostream>
#include <fstream>
#include <cstdlib>
#include <assert.h>
#include "gpu_asm.hpp"

using namespace std;


int main(int argc, char* argv[])
{
	system("cpp r800.def > r800.def.ii");
	ifstream f("r800.def.ii");
	
	string text = std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	
	gpu_asm::asm_definition asmdef(text);
	
	gpu_assembler assembler(asmdef);
	
	vector<uint32_t> code;/* = 
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
	};*/
	
// 	FILE *f4 = fopen("gpu_bin.bin", "r");
// 	FILE *f4 = fopen("ki.dat", "r");
	
	FILE *f4 = fopen(argv[1], "r");
	assert(f4);
	uint32_t val;
	
	fseek(f4, 0x0, SEEK_SET);
	
	while (fread(&val, 1, sizeof(uint32_t), f4) > 0)
	{
		code.push_back(val);
	}
	
	for (int i = 0; i < code.size(); i+=4)
	{
		printf("%i : 0x%.8X 0x%.8X 0x%.8X 0x%.8X\n", i, code[i], code[i+1], code[i+2], code[i+3]);
	}
	
	fclose(f4);
	
// 	code = gpu_asm::byte_mirror(code);
	
	
	gpu_disassembler dis(asmdef);
	
// 	cout << "disassemble: " << endl 
	cout << dis.disassemble(code) << endl << "end;" << endl;
	
	ifstream f2("test.asm");
	
	auto codes = assembler.assemble(std::string(std::istreambuf_iterator<char>(f2), std::istreambuf_iterator<char>()));
	
	for (int i = 0; i < codes.size(); i+=4)
	{
// 		printf("%i : 0x%.8X 0x%.8X 0x%.8X 0x%.8X\n", i, codes[i], codes[i+1], codes[i+2], codes[i+3]);
	}
	
	FILE *f3 = fopen("ki.dat", "w");
	
	fwrite(&codes[0], 1, sizeof(uint32_t)*codes.size(), f3);
	
	fclose(f3);
}
