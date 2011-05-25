#include <map>
#include <iostream>
#include <stdexcept>
#include <assert.h>
#include <sstream>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include "gpu_asm.hpp"
#include "asm_parser.hpp"

using namespace std;

gpu_assembler::gpu_assembler(const gpu_asm::asm_definition& asmdef) : asmdef(asmdef)
{
	
}
	
std::vector<uint32_t> gpu_assembler::assemble(std::string text)
{
	std::vector<uint32_t> data;
	std::vector<gpu_asm::instruction> raw_stream = parse_asm_text(text);
	labels.clear();
	parsed_instructions.clear();
	
	string cf_prefix = "CF_";
	string alu_prefix = "ALU_";
	string vtx_prefix = "VTX_";
	string tex_prefix = "TEX_";
	string gds_prefix = "MEM_GDS_";
	
	//split parsed instructions into data and control flow
	for (int i = 0; i < int(raw_stream.size()); i++)
	{
		gpu_asm::instruction instr = raw_stream[i];
		
		if (instr.label != "")
		{
			labels[instr.label] = -1; //reserve label
		}
		
		if (asmdef.microcode_format_tuples.count(instr.name))
		{
			auto tuple = asmdef.microcode_format_tuples.at(instr.name);
			
			if (tuple.tuple.front().substr(0, cf_prefix.size()) == cf_prefix)
			{
				cf_instruction cfi;
				
				cfi.instr = instr;
				parsed_instructions.push_back(cfi);
			}
			else
			{
				if (parsed_instructions.size() == 0)
				{
					throw runtime_error("Error: the program seems to be starting with a dataflow instruction, which lacks control flow");
				}
				
				parsed_instructions.back().df_clause.push_back(instr);
			}
		}
		else
		{
			throw runtime_error("Undefine instruction: " + instr.name);
		}
	}
	
	//dummy generate control flow to get address of labels, and size of the control flow
	
	int dword_pos = 0;
	
	for (int i = 0; i < int(parsed_instructions.size()); i++)
	{
		auto instr = parsed_instructions[i].instr;
		
		if (instr.label != "")
		{
			
			labels[instr.label] = dword_pos;
		}
		
		assert(dword_pos%2 == 0);
		
		parsed_instructions[i].cf_pos = dword_pos;
		parsed_instructions[i].cf_codes = assemble_instruction(instr);
		dword_pos += parsed_instructions[i].cf_codes.size();
	}
		
	//generate data flow clauses, separately attributed to specific control flow instructions
	
	for (int i = 0; i < int(parsed_instructions.size()); i++)
	{
		vector<uint32_t> codes;
		
		for (int j = 0; j < int(parsed_instructions[i].df_clause.size()); j++)
		{
			std::vector<uint32_t> code = assemble_instruction(parsed_instructions[i].df_clause[j]);
			codes.insert(codes.end(), code.begin(), code.end());
		}
		
		parsed_instructions[i].df_codes = codes;
	}
	
	//allocate data flow clauses after the control flow
	
	int data_flow_index = dword_pos+20;
	
	//first allocate ALU clauses they only need normal 64 alignment (we hope)
	for (int i = 0; i < int(parsed_instructions.size()); i++)
	{
		if (parsed_instructions[i].df_codes.size())
		{
			auto tuple = asmdef.microcode_format_tuples.at(parsed_instructions[i].df_clause.front().name);
			
			if (tuple.tuple.front().substr(0, alu_prefix.size()) == alu_prefix)
			{
				data_flow_index += data_flow_index%2; //aliging to 64 bits
/*				if (data_flow_index%4)
				{
					data_flow_index += 4-data_flow_index%4; //align 128 bits
				}*/
				printf("alu:%x\n", data_flow_index*4);
				parsed_instructions[i].df_pos = data_flow_index;
				data_flow_index += parsed_instructions[i].df_codes.size();
			}
		}
	}
	
	data_flow_index += 20; //For some undocumented reason the reference code has this padding
	
	//Than allocate fetch clauses they need normal 128 alignment
	for (int i = 0; i < int(parsed_instructions.size()); i++)
	{
		if (parsed_instructions[i].df_codes.size())
		{
			auto tuple = asmdef.microcode_format_tuples.at(parsed_instructions[i].df_clause.front().name);
			
			if (tuple.tuple.front().substr(0, vtx_prefix.size()) == vtx_prefix)
			{
				if (data_flow_index%4)
				{
					data_flow_index += 4-data_flow_index%4; //align 128 bits
				}
				printf("vtx:%x\n", data_flow_index*4);
				parsed_instructions[i].df_pos = data_flow_index;
				data_flow_index += parsed_instructions[i].df_codes.size();
			}
		}
	}
	
	for (int i = 0; i < int(parsed_instructions.size()); i++)
	{
		if (parsed_instructions[i].df_codes.size())
		{
			auto tuple = asmdef.microcode_format_tuples.at(parsed_instructions[i].df_clause.front().name);
			
			if (tuple.tuple.front().substr(0, tex_prefix.size()) == tex_prefix)
			{
				if (data_flow_index%4)
				{
					data_flow_index += 4-data_flow_index%4; //align 128 bits
				}
				printf("tex:%x\n", data_flow_index*4);
				parsed_instructions[i].df_pos = data_flow_index;
				data_flow_index += parsed_instructions[i].df_codes.size();
			}
		}
	}
	
// 	data_flow_index += 20;

	for (int i = 0; i < int(parsed_instructions.size()); i++)
	{
		if (parsed_instructions[i].df_codes.size())
		{
			auto tuple = asmdef.microcode_format_tuples.at(parsed_instructions[i].df_clause.front().name);
			
			if (tuple.tuple.front().substr(0, gds_prefix.size()) == gds_prefix)
			{
				if (data_flow_index%4)
				{
					data_flow_index += 4-data_flow_index%4; //align 128 bits
				}
				
				printf("gds: %x\n", data_flow_index*4);
				parsed_instructions[i].df_pos = data_flow_index;
				data_flow_index += parsed_instructions[i].df_codes.size();
			}
		}
	}

	data_flow_index += 4-data_flow_index%4; //end padding
	
	//fill ADDR and COUNT fields of the control flow in the parsed format, to refer to the clauses

	for (int i = 0; i < int(parsed_instructions.size()); i++)
	{
		if (parsed_instructions[i].df_codes.size())
		{
			int dv = 2;
			auto tuple = asmdef.microcode_format_tuples.at(parsed_instructions[i].df_clause.front().name);
			
			if (tuple.tuple.front().substr(0, alu_prefix.size()) == alu_prefix)
			{
				dv = 2; //size of the slot for ALU clauses
			}
			else
			{
				dv = 4; //size of the instruction for Texture of Vertex clauses
			}
			
			parsed_instructions[i].instr.fields.push_back(gpu_asm::microcode_field("ADDR", parsed_instructions[i].df_pos/2));
			parsed_instructions[i].instr.fields.push_back(gpu_asm::microcode_field("COUNT", parsed_instructions[i].df_codes.size()/dv - 1));
// 			cout << parsed_instructions[i].instr.name << " " << parsed_instructions[i].df_codes.size() << endl;
		}
	}
	
	//generate final control flow code
	
	for (int i = 0; i < int(parsed_instructions.size()); i++)
	{
		auto instr = parsed_instructions[i].instr;
		
		parsed_instructions[i].cf_codes = assemble_instruction(instr);
	}
	
	//copy clauses into their final place
	
	data.resize(data_flow_index);
	
	for (int i = 0; i < int(parsed_instructions.size()); i++)
	{
		copy(parsed_instructions[i].cf_codes.begin(), parsed_instructions[i].cf_codes.end(), data.begin()+parsed_instructions[i].cf_pos);
		
		if (parsed_instructions[i].df_clause.size())
		{
			copy(parsed_instructions[i].df_codes.begin(), parsed_instructions[i].df_codes.end(), data.begin()+parsed_instructions[i].df_pos);
		}
	}
	
	return data;
}


std::vector<uint32_t> gpu_assembler::assemble_instruction(gpu_asm::instruction instr)
{
	std::vector<uint32_t> data;
	
	if (asmdef.microcode_format_tuples.count(instr.name) == 0)
	{
		throw runtime_error("Undefine instruction: " + instr.name);
	}
	
	auto tuple = asmdef.microcode_format_tuples.at(instr.name);
	
	for (auto i = tuple.constraints.begin(); i != tuple.constraints.end(); i++)
	{
		gpu_asm::microcode_field field(i->first.second, 0);
		field.enum_elem = i->second;
		instr.fields.push_back(field);
	}
	
	int instr_lit_num = 0;
	int tuple_lit_num = 0;
	
	for (int i = 0; i < int(instr.fields.size()); i++)
	{
		if (instr.fields[i].name.substr(0, 2) == "0x")
		{
			instr_lit_num++;
		}
	}
	
	for (int i = 0; i < int(tuple.tuple.size()); i++)
	{
		if (asmdef.microcode_formats.at(tuple.tuple[i]).name.find("LITERAL_CONSTANT") != string::npos)
		{
			tuple_lit_num++;
		}
	}
	
	if (instr_lit_num < tuple_lit_num)
	{
		throw runtime_error("Not enough literals in instruction: " + instr.name);
	}
	
	data.resize(tuple.size_in_bits / 32 + instr_lit_num - tuple_lit_num);
	
	vector<int> literal_mapping;
	
	for (int i = 0; i < int(tuple.tuple.size()); i++)
	{
		if (asmdef.microcode_formats.at(tuple.tuple[i]).name.find("LITERAL_CONSTANT") != string::npos)
		{
			literal_mapping.push_back(i);
		}
	}
	
	for (int i = tuple.size_in_bits / 32; i < int(data.size()); i++)
	{
		literal_mapping.push_back(i);
	}
	
	assert(int(literal_mapping.size()) == instr_lit_num);
	
	
	assemble_literals(data, literal_mapping, instr);
	
	assemble_fields(data, instr);
	
	return data;
}

void gpu_assembler::assemble_literals(std::vector<uint32_t>& data, const std::vector<int>& literal_mapping, gpu_asm::instruction instr)
{
	int lit_idx = 0;
	
	for (int i = 0; i < int(instr.fields.size()); i++)
	{
		if (instr.fields[i].name.substr(0, 2) == "0x")
		{
			uint32_t value = 0;
			
			int e = sscanf(instr.fields[i].name.c_str(), "%x", &value);
			
			if (e < 1)
			{
				throw runtime_error("Parsing error at parsing literal: " + instr.fields[i].name);
			}
			
			data[literal_mapping[lit_idx]] = value;
			
			lit_idx++;
		}
	}
}

void gpu_assembler::assemble_fields(std::vector<uint32_t>& data, gpu_asm::instruction instr)
{
	for (int i = 0; i < int(instr.fields.size()); i++)
	{
		if (instr.fields[i].name.substr(0, 2) == "0x")
		{
			continue;
		}
		
		int pos = 0;
		gpu_asm::field field_def = get_field_def(instr, instr.fields[i], pos);
		
		uint32_t mask = gen_field_mask(field_def, instr.fields[i], instr);
		
		data[pos] |= mask;
	}
}

uint32_t gpu_assembler::gen_field_mask(gpu_asm::field field_def, gpu_asm::microcode_field field, gpu_asm::instruction instr)
{
	if (field_def.numeric)
	{
		if (field.enum_elem != "")
		{
			throw runtime_error("Field is an INT not an enum: " + instr.name + "." + field.name + " invalid enum elem: \"" + field.enum_elem + "\"");
		}
		
		if (field.label != "")
		{
			if (labels.count(field.label) == 0)
			{
				throw runtime_error("Undefined label: \"" + field.label + "\" in: " + instr.name + "." + field.name);
			}
			
			field.offset = labels.at(field.label)/2;
		}
		
		return gen_mask(field.offset, field_def, instr);
	}
	
	if (field_def.flag)
	{
		if (field.enum_elem != "")
		{
			throw runtime_error("Field is a BOOL not an enum: " + instr.name + "." + field.name + " invalid enum elem: \"" + field.enum_elem + "\"");
		}
		
		if (field.offset_is_set)
		{
			throw runtime_error("Field is a BOOL not an INT, you should not set its value: " + instr.name + "." + field.name);
		}
		
		return gen_mask(1, field_def, instr);
	}
	
	if (field_def.vals.size())
	{
		gpu_asm::enum_val enum_val;
		enum_val.name = field.enum_elem;
		
		if (field_def.vals.count(enum_val) == 0)
		{
			throw runtime_error("Enum elem is undefined in field: " + instr.name + "." + field.name + " invalid enum elem: \"" + field.enum_elem + "\"");
		}
		
		enum_val = *field_def.vals.find(enum_val);
		
		long value = min(enum_val.value_bound.start, enum_val.value_bound.stop);
		long max_val = max(enum_val.value_bound.start, enum_val.value_bound.stop);
		
		if (field.offset_is_set)
		{
			value += field.offset;
			
			if (value > max_val)
			{
				stringstream ss;
				ss << "Enum offset is out of bounds: "+ instr.name + "." + field.name + "." + field.enum_elem  << " offset: " << field.offset << endl;
				
				throw runtime_error(ss.str());
			}
		}
		
		return gen_mask(value, field_def, instr);
	}
	
	throw runtime_error("Invalid field in the definition: " + field_def.name);
}

uint32_t gpu_assembler::gen_mask(long value, gpu_asm::field field_def, gpu_asm::instruction instr)
{
	int start = min(field_def.bits.start, field_def.bits.stop);
	int stop = max(field_def.bits.start, field_def.bits.stop);
	int len = stop-start+1;
	long maxval = pow(2, len)-1;
	
// 	cout << value << " " << maxval << endl;
	if (value > maxval)
	{
		throw runtime_error("Value is out of bounds in: " + instr.name + "." + field_def.name);
	}
	
	uint32_t mask = value << start;
	
	return mask;
}


gpu_asm::field gpu_assembler::get_field_def(gpu_asm::instruction instr, gpu_asm::microcode_field field, int& pos)
{
	gpu_asm::microcode_format_tuple tuple = asmdef.microcode_format_tuples.at(instr.name);
	
	for (int i = 0; i < int(tuple.tuple.size()); i++)
	{
		gpu_asm::microcode_format format = asmdef.microcode_formats.at(tuple.tuple[i]);
		
		for (int j = 0; j < int(format.fields.size()); j++)
		{
			if (field.name == format.fields[j].name)
			{
				pos = i;
				return format.fields[j];
			}
		}
	}
	
	throw runtime_error("Undefined field: " + instr.name + "." + field.name);
}



gpu_disassembler::gpu_disassembler(const gpu_asm::asm_definition& asmdef)
	: asmdef(asmdef), indent(0)
{
}

std::string gpu_disassembler::disassemble(std::vector<uint32_t> data)
{
	disassemble_cf(data); //prepare labels!
	
// 	cout << endl << endl << endl;
	
	return disassemble_cf(data);
}

std::string gpu_disassembler::disassemble_cf(std::vector<uint32_t> data)
{
	vector<uint32_t> orig_data = data;
	std::string result;
	int orig_size = data.size();
	
	int match_num;
	
	filter_prefix = "CF_";
	
	set<int> literal_chan_read;
	
	do{
		map<string, int> tuple_matches;
		match_num = 0;
		int match_size = 0;
		gpu_asm::microcode_format_tuple match_tuple;
		
// 		cerr << orig_size - data.size() << endl;

		for (auto i = asmdef.microcode_format_tuples.begin(); i != asmdef.microcode_format_tuples.end(); i++)
		{

			if (filter_prefix != i->second.tuple.front().substr(0, filter_prefix.size()))
			{
				continue;
			}
			
			tuple_matches[i->first] = try_tuple_fit(data, i->second);
			
			if (tuple_matches[i->first])
			{
				match_num++;
				match_size = tuple_matches[i->first] / 32;
				match_tuple = i->second;
			}
		}
	
// 		cerr << match_tuple.name << endl;
		
		if (match_num > 1)
		{
			cerr << "Ambiguous matches at " << orig_size - data.size() << endl;
			
			for (auto i = tuple_matches.begin(); i != tuple_matches.end(); i++)
			{
				if (i->second)
					cerr << i->first << endl;
			}
			
			throw runtime_error("Ambiguous matches");
		}
		
		if (match_num == 1)
		{
			result += parse_tuple(data, match_tuple);
		
			result += "\n";
			
// 			if (match_tuple.tuple[0] == "CF_WORD0")
			if (check_field(data, match_tuple, "END_OF_PROGRAM", "", false))
			{
				break;
			}
			
			data.erase(data.begin(), data.begin() + match_size);
		}
		
// 		cout << result << endl;
		
		if (clause_todo.size())
		{
			indent = 1;
			result += disassemble_clause(orig_data, clause_todo.front());
			indent = 0;
			clause_todo.clear();
		}
		
		int index = (orig_data.size() - data.size());
		
		if (label_table.count(index/2))
		{
			stringstream ss;
			
			ss << "@" << label_table[index/2] << endl;
			result += ss.str();
		}
		
/*		{
			stringstream ss;
			ss << "//" << index/2 << endl;
			result += ss.str();
		}*/
		
		if (data.size() == 0)
		{
// 			cout << "zero size reached" << endl;
			return result;
		}
		
		if (match_num == 0)
		{
			cerr << "Disassembling error at dword: " << orig_size - data.size() << endl;
			printf("0x%.8X\n", data[0]);
			
			if (data.size() > 1)
			{
				printf("0x%.8X\n", data[1]);
			}
			
			throw runtime_error("Disassembling error");
		}
		
		
	} while (match_num);
	
// 	if (data.size() != 0)
// 	{
// 		cerr << result << endl;
// 		cerr << "Disassembling error at dword: " << orig_size - data.size() << endl;
// 		printf("0x%.8X\n", data[0]);
// 		if (data.size() > 1)
// 		{
// 			printf("0x%.8X\n", data[1]);
// 		}
// 		throw runtime_error("Disassembling error");
// 	}
	
	return result;
}

std::string gpu_disassembler::disassemble_clause(std::vector<uint32_t> data, tclause clause)
{
	std::string result;
	int orig_size = data.size();
	
	int match_num;
	
	clause.len++;
	
	filter_prefix = clause.prefix + "_";
	
	set<int> literal_chan_read;
	
// 	cout << data.size() << " " << clause.addr << " " << clause.len << " " << clause.prefix << endl;
	
	data.erase(data.begin(), data.begin()+clause.addr*2);
	
// 	printf("%.8x\n", data.front());
	
	string filter_prefix2 = filter_prefix;
	
	if (filter_prefix == "TEX_")
	{
		filter_prefix2 = "VTX_";
	}
	
	if (filter_prefix == "VTX_")
	{
		filter_prefix2 = "TEX_";
	}
	
// 	cout << filter_prefix2 << endl;
	
// 	cout << clause.addr*2 << " ";
	
	for (auto i = label_table.begin(); i != label_table.end(); i++)
	{
// 		cout << i->first << " ";
	}
	
// 	cout << endl;
	
	
	for (int icount = 0; icount < clause.len; icount++)
	{
		map<string, int> tuple_matches;
		match_num = 0;
		int match_size = 0;
		gpu_asm::microcode_format_tuple match_tuple;
		
		for (auto i = asmdef.microcode_format_tuples.begin(); i != asmdef.microcode_format_tuples.end(); i++)
		{
			if (filter_prefix != i->second.tuple.front().substr(0, filter_prefix.size()) and 
				filter_prefix2 != i->second.tuple.front().substr(0, filter_prefix2.size()))
			{
				continue;
			}
			
			tuple_matches[i->first] = try_tuple_fit(data, i->second);
			
			if (tuple_matches[i->first])
			{
				match_num++;
				match_size = tuple_matches[i->first] / 32;
				match_tuple = i->second;
			}
		}
		
		cerr << filter_prefix << match_tuple.name << endl;
		if (match_tuple.name == "MEM_GDS")
		{
			cerr << check_field(data, match_tuple, "MEM_OP") << endl;
		}
		
		if (match_num > 1)
		{
			cerr << "Ambiguous matches at " << orig_size - data.size() << endl;
			
			for (auto i = tuple_matches.begin(); i != tuple_matches.end(); i++)
			{
				if (i->second)
					cerr << i->first << endl;
			}
			
			throw runtime_error("Ambiguous matches");
		}
		
		if (match_num == 1)
		{
			string p_str = parse_tuple(data, match_tuple);
			
			result += p_str;
			
			if (match_tuple.tuple[0] == "ALU_WORD0")
			{
				if (check_field(data, match_tuple, "SRC0_SEL", "ALU_SRC_LITERAL"))
				{
					literal_chan_read.insert(check_field(data, match_tuple, "SRC0_CHAN"));
				}
				
				if (check_field(data, match_tuple, "SRC1_SEL", "ALU_SRC_LITERAL"))
				{
					literal_chan_read.insert(check_field(data, match_tuple, "SRC1_CHAN"));
				}
				
				if (check_field(data, match_tuple, "LAST"))
				{
					int lit_size = 0;
					
					if (literal_chan_read.count(0) ||  literal_chan_read.count(1))
					{
						lit_size = 2;
					}
					
					if (literal_chan_read.count(2) ||  literal_chan_read.count(3))
					{
						lit_size = 4;
					}
					
					icount += lit_size/2;
					
					result += parse_literals(data, match_size, lit_size);
					
					literal_chan_read.clear();
					
					match_size += lit_size;
				}
			}
			
			
			data.erase(data.begin(), data.begin() + match_size);
			result += "\n";
			
// 			if (match_tuple.tuple[0] == "ALU_WORD0")
// 			if (check_field(data, match_tuple, "LAST"))
// 			{
// 				cout << p_str << endl;
// 				cout << "LAST found" << endl;
// 				break;
// 			}
		}
		
		if (match_num == 0)
		{
			cerr << "Undefined instruction: " << endl;
			
			printf("%.8X\n", data[0]);
			if (data.size() > 1)
			{
				printf("%.8X\n", data[1]);
			}
			throw runtime_error("Disassembling error");
		}
		
// 		cout << result << endl;
		
	}
	
/*	if (data.size() != 0)
	{
		cerr << result << endl;
		cerr << "Disassembling error at dword: " << orig_size - data.size() << endl;
		printf("0x%.8X\n", data[0]);
		if (data.size() > 1)
		{
			printf("0x%.8X\n", data[1]);
		}
		throw runtime_error("Disassembling error");
	}*/
	
	filter_prefix = "CF_";
	return result;
}

gpu_asm::field gpu_disassembler::get_field(std::string format_name, std::string field_name)
{
	auto format = asmdef.microcode_formats.at(format_name);
	
	for (int i = 0; i < int(format.fields.size()); i++)
	{
		if (format.fields[i].name == field_name)
		{
			return format.fields[i];
		}
	}
	
	throw runtime_error("Internal error in get_field: " + format_name + "." + field_name + " not found");
}

long gpu_disassembler::get_field_value(uint32_t code, gpu_asm::bound bound)
{
	uint32_t mask = 0;
	int shift = 0;
	
	
	shift = std::min(bound.start, bound.stop);
	
	for (int i = std::min(bound.start, bound.stop); i <= std::max(bound.start, bound.stop); i++)
	{
		mask += uint32_t(1) << i; 
	}
	
// 	printf("get field_value: %.8X, (%i, %i), mask: %.8X, shift: %i\n", code, bound.start, bound.stop, mask, shift);
	return (code & mask) >> shift;
}

std::string gpu_disassembler::get_enum_value(uint32_t code, gpu_asm::field field)
{
	assert(field.enum_name != "");
	long num_val = get_field_value(code, field.bits);
	
// 	cout << "get enum value: " << field.enum_name << ":" << num_val << endl;
	
	for (auto i = field.vals.begin(); i != field.vals.end(); i++)
	{
		long start = min(i->value_bound.start, i->value_bound.stop);
		long stop = max(i->value_bound.start, i->value_bound.stop);
		
		if (num_val >= start and num_val <= stop)
		{
			string result = i->name;
			
			if (start != stop)
			{
				stringstream ss;
				ss << "(" << num_val - start << ")";
				result += ss.str();
			}
			
// 			cout << num_val << " " << start << " " <<  stop << " res: " << result << endl;
			
			return result;
		}
	}
	
	if (num_val == 0)
		return "";
	
	stringstream ss;
	
	ss << "Parsed value cannot be represented in enum, at field: " << field.name << " numeric value: " << num_val;
	
	throw runtime_error(ss.str());
}

int gpu_disassembler::try_tuple_fit(const std::vector<uint32_t>& data, const gpu_asm::microcode_format_tuple& tuple)
{
	if (int(data.size()) < tuple.size_in_bits / 32)
		return 0;
	
	map<string, uint32_t> codes;
	
	for (int i = 0; i < int(tuple.tuple.size()); i++)
	{
		codes[tuple.tuple[i]] = data[i];
	}
	
	for (auto i = tuple.constraints.begin(); i != tuple.constraints.end(); i++)
	{
		gpu_asm::field field = get_field(i->first.first, i->first.second);
		
		long value = get_field_value(codes[i->first.first], field.bits);
		
		gpu_asm::enum_val enval;
		enval.name = i->second;
		
		assert(field.vals.count(enval));
		
		enval = *field.vals.find(enval);
		
		if (enval.value_bound.start != enval.value_bound.stop)
		{
			throw runtime_error("Error enum value in the constraint is a range: " + i->first.first + "." + i->first.second + " == " + i->second);
		}
		
		if (value != enval.value_bound.start)
			return 0;
	}
	
	return tuple.size_in_bits;
}

std::string gpu_disassembler::parse_tuple(const std::vector<uint32_t>& data, const gpu_asm::microcode_format_tuple& tuple)
{
	std::string result;
	
	result = gen_indent(0) + tuple.name + ":\n";
	
	for (int i = 0; i < int(tuple.tuple.size()); i++)
	{
		auto m_format = asmdef.microcode_formats.at(tuple.tuple[i]);
		
		if (m_format.name.find("LITERAL_CONSTANT") != string::npos)
		{
			result += parse_literals(data, i, m_format.size_in_bits/32);
		}
		else
		{
			string parsed_micro = parse_microcode(data[i], m_format, tuple);
			
			if (parsed_micro != "")
			{
				result += gen_indent(1) + parsed_micro + ";\n";
			}
		}
	}
	
	return result;
}

std::string gpu_disassembler::parse_microcode(uint32_t code, const gpu_asm::microcode_format& format, const gpu_asm::microcode_format_tuple& tuple)
{
	std::string result;
	
	for (int i = 0; i < int(format.fields.size()); i++)
	{
		if (tuple.constraints.count(make_pair(format.name, format.fields[i].name)))
		{
			if (tuple.constraints.at(make_pair(format.name, format.fields[i].name)) != "")
			{
				continue;
			}
		}
		
		string str_field = parse_field(code, format.fields[i], tuple);
		
		if (result != "" and str_field != "")
			result += " ";
		
		result += str_field;
	}
	
/*	if (result != "")
	{
		result = format.name + "> " + result;
	}*/
	
	return result;
}

std::string gpu_disassembler::parse_field(uint32_t code, gpu_asm::field field, gpu_asm::microcode_format_tuple tuple)
{
	string fname = field.name;
	bool code_addr = false;
	string code_addr_type = "";
	bool nodefault = false;
	string s_name = fname + "_MEANS_";
	string code_addr_prefix = fname + "_IS_";
	
	for (auto i = tuple.options.begin(); i != tuple.options.end(); i++)
	{
		if (i->substr(0, s_name.size()) == s_name)
		{
			fname = i->substr(s_name.size(), i->size());
// 			break;
		}
		
		if (i->substr(0, code_addr_prefix.size()) == code_addr_prefix)
		{
			code_addr_type = i->substr(code_addr_prefix.size(), i->size());
			code_addr = true;
// 			break;
		}
		
		if (*i == (fname+"_NODEFAULT"))
		{
// 			cerr << "nodefault: " << fname << endl;
			nodefault = true;
		}
		
		if (*i == (fname+"_IMPLICIT"))
		{
			return "";
		}
	}
	
	if (fname == "NOTHING")
		return "";
	
	if (code_addr and !field.numeric)
	{
		throw runtime_error("Only numeric fields are allowed to contain an address: " + field.name + " tuple: " + tuple.name);
	}
	
	if (field.numeric)
	{
		long value = get_field_value(code, field.bits);
		
		long start = min(field.numeric_bounds.start, field.numeric_bounds.stop);
		long stop = max(field.numeric_bounds.start, field.numeric_bounds.stop);
		
		if (value < start or value > stop)
		{
			stringstream ss;
			ss << "Numeric value" << value << "is out of bounds " << start << ":" << stop << " at field: " << field.name;
			throw runtime_error(ss.str());
		}
		
		stringstream ss;
		
		if (code_addr)
		{
			
			tclause clause;
			clause.addr = value;
			clause.prefix = code_addr_type;
			clause.len = -1; //should be filled later if applicable
			
// 			cout << code_addr_type << " " << ss.str() << endl;
			if (code_addr_type != "CF")
			{
				clause_todo.push_back(clause);
				return "";
			}
			
			if (label_table.count(value) == 0)
			{
				label_table[value] = label_table.size();
			}
			
			ss << fname << "(@" << label_table.at(value) << ")"; 
		} 
		else if (value or nodefault)
		{
			ss << fname << "(" << value << ")";
		}
		
		if (fname == "COUNT" and clause_todo.size())
		{
			clause_todo.back().len = value;
			return "";
		}
		
		return ss.str();
	}
	
	if (field.flag)
	{
		long value = get_field_value(code, field.bits);
		
		if (value)
		{
			return fname;
		}
		
		return "";
	}
	
	if (field.enum_name != "")
	{
		string enum_eval = get_enum_value(code, field);
		
		if (get_field_value(code, field.bits) == 0)
		{
			for (auto i = field.vals.begin(); i != field.vals.end(); i++)
			{
				if (i->value_bound.start == 0 and i->value_bound.stop == 0 and i->default_option)
				{
					return "";
				}
			}
		}
		
		if (enum_eval != "")
		{
			return fname + "." + enum_eval;
		}
		
		return "";
	}
	
	throw runtime_error("internal error: field has no valid type: " + field.name);
}

uint32_t gpu_asm::byte_mirror(uint32_t val)
{ 
	uint32_t b1, b2, b3, b4;
	
	b1 = val & 0x000000FF;
	b2 = val & 0x0000FF00;
	b3 = val & 0x00FF0000;
	b4 = val & 0xFF000000;
	
	b1 = b1 << 24;
	b2 = b2 << 8;
	b3 = b3 >> 8;
	b4 = b4 >> 24;
	
	return b1 | b2 | b3 | b4;
}

std::vector<uint32_t> gpu_asm::byte_mirror(std::vector<uint32_t> codes)
{
	for (int i = 0; i < int(codes.size()); i++)
	{
		codes[i] = byte_mirror(codes[i]);
		printf("%.8X\n", codes[i]);
	}
	
	return codes;
}

long gpu_disassembler::check_field(const std::vector<uint32_t>& data, const gpu_asm::microcode_format_tuple& tuple, std::string field_name, std::string field_value, bool strict)
{
	int offset = 0;
	string elem_name;
	
	parse_field_value(field_value, offset, elem_name);
	
	for (int i = 0; i < int(tuple.tuple.size()); i++)
	{
		for (int j = 0; j < int(asmdef.microcode_formats[tuple.tuple[i]].fields.size()); j++)
		{
			gpu_asm::field field = asmdef.microcode_formats[tuple.tuple[i]].fields[j];
			
			if (field.name == field_name)
			{
				long val = get_field_value(data[i], field.bits);
				
				if (field_value == "")
					return val;
				
				long ref_val = offset;
				
				for (auto k = field.vals.begin(); k != field.vals.end(); k++)
				{
					if (k->name == elem_name)
					{
						ref_val += min(k->value_bound.start, k->value_bound.stop);
						
						break;
					}
				}
				
				return ref_val == val;
			}
		}
	}
	
	if (strict)
	{
		throw runtime_error("Field not found: " + field_name);
	}
	
	return 0;
}

std::string gpu_disassembler::parse_literals(const std::vector<uint32_t>& data, int offset, int size)
{
	char buf[100];
	int pos = 0;
	
	for (int i = offset; i < offset+size; i++)
	{
		pos += sprintf(buf+pos, "%s0x%.8X;\n", gen_indent(1).c_str(), data[i]);
	}
	
	return buf;
}

std::string gpu_disassembler::gen_indent(int offset)
{
	std::string s;
	
	for (int i = 0; i < indent+offset; i++)
	{
		s += "\t";
	}
	
	return s;
}
