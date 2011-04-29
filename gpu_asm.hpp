#ifndef GPU_ASM_HPP
#define GPU_ASM_HPP
#include <vector>
#include <string>
#include "gpu_asm_def.hpp"

class gpu_assembler
{
	gpu_asm::asm_definition asmdef;
	std::vector<gpu_asm::instruction> parsed_instructions;
	
public:
	gpu_assembler(const gpu_asm::asm_definition& asmdef);
	std::vector<uint32_t> assemble(std::string text);
};

class gpu_disassembler
{
	gpu_asm::asm_definition asmdef;
	
public:
	gpu_disassembler(const gpu_asm::asm_definition& asmdef);
	std::string disassemble(std::vector<uint32_t> data);
	gpu_asm::field get_field(std::string format_name, std::string field_name);
	long get_field_value(uint32_t code, gpu_asm::bound bound);
	std::string get_enum_value(uint32_t code, gpu_asm::field);
	int try_tuple_fit(const std::vector<uint32_t>& data, const gpu_asm::microcode_format_tuple& tuple);
	std::string parse_tuple(const std::vector<uint32_t>& data, const gpu_asm::microcode_format_tuple& tuple);
	std::string parse_microcode(uint32_t code, const gpu_asm::microcode_format& format);
	std::string parse_field(uint32_t code, gpu_asm::field);
};

#endif
