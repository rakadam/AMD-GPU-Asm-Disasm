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
	std::vector<uint32_t> assemble_instruction(gpu_asm::instruction);
	void assemble_literals(std::vector<uint32_t>& data, const std::vector<int>& literal_mapping, gpu_asm::instruction);
	void assemble_fields(std::vector<uint32_t>& data, gpu_asm::instruction);
	gpu_asm::field get_field_def(gpu_asm::instruction, gpu_asm::microcode_field, int& pos);
	uint32_t gen_field_mask(gpu_asm::field, gpu_asm::microcode_field, gpu_asm::instruction);
	uint32_t gen_mask(long value, gpu_asm::field, gpu_asm::instruction);
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
	std::string parse_microcode(uint32_t code, const gpu_asm::microcode_format& format, const gpu_asm::microcode_format_tuple& tuple);
	std::string parse_field(uint32_t code, gpu_asm::field);
	
	long check_field(const std::vector<uint32_t>& data, const gpu_asm::microcode_format_tuple& tuple, std::string field_name, std::string field_value = ""); //field_value == "": returns the numeric val of the field, otherwise returns a boolean
	
	std::string parse_literals(const std::vector<uint32_t>& data, int offset, int size);
};

namespace gpu_asm
{
	uint32_t byte_mirror(uint32_t);
	std::vector<uint32_t> byte_mirror(std::vector<uint32_t>);
};
#endif
