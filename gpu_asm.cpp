#include <map>
#include <iostream>
#include <stdexcept>
#include <assert.h>
#include <sstream>
#include <cstdio>
#include "gpu_asm.hpp"
#include "asm_parser.hpp"

using namespace std;

gpu_assembler::gpu_assembler(const gpu_asm::asm_definition& asmdef) : asmdef(asmdef)
{
	
}
	
std::vector<uint32_t> gpu_assembler::assemble(std::string text)
{
	parsed_instructions = parse_asm_text(text);
	
	return {};
}



gpu_disassembler::gpu_disassembler(const gpu_asm::asm_definition& asmdef)
	: asmdef(asmdef)
{
}

std::string gpu_disassembler::disassemble(std::vector<uint32_t> data)
{
	std::string result;
	int orig_size = data.size();
	
	int match_num;
	
	set<int> literal_chan_read;
	
	do{
		map<string, int> tuple_matches;
		match_num = 0;
		int match_size = 0;
		gpu_asm::microcode_format_tuple match_tuple;
		
		for (auto i = asmdef.microcode_format_tuples.begin(); i != asmdef.microcode_format_tuples.end(); i++)
		{
			tuple_matches[i->first] = try_tuple_fit(data, i->second);
			
			if (tuple_matches[i->first])
			{
				match_num++;
				match_size = tuple_matches[i->first] / 32;
				match_tuple = i->second;
			}
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
// 			cout << parse_tuple(data, match_tuple) << endl;
			result += parse_tuple(data, match_tuple);
		
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
					
					result += parse_literals(data, match_size, lit_size);
					
					literal_chan_read.clear();
					
					match_size += lit_size;
				}
			}
			
			data.erase(data.begin(), data.begin() + match_size);
			result += "\n";
		}
	} while (match_num);
	
	if (data.size() != 0)
	{
		cerr << result << endl;
		cerr << "Disassembling error at dword: " << orig_size - data.size() << endl;
		printf("0x%.8X\n", data[0]);
		throw runtime_error("Disassembling error");
	}
	
	
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
	
	result = tuple.name + ":\n";
	
	for (int i = 0; i < int(tuple.tuple.size()); i++)
	{
		auto m_format = asmdef.microcode_formats.at(tuple.tuple[i]);
		
		if (m_format.name.find("LITERAL_CONSTANT") != string::npos)
		{
			result += parse_literals(data, i, m_format.size_in_bits/32);
		}
		else
		{
			result += "\t" + parse_microcode(data[i], m_format, tuple) + ";\n";
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
		
		string str_field = parse_field(code, format.fields[i]);
		
		if (result != "" and str_field != "")
			result += " ";
		
		result += str_field;
	}
	
	if (result != "")
	{
		result = format.name + "> " + result;
	}
	
	return result;
}

std::string gpu_disassembler::parse_field(uint32_t code, gpu_asm::field field)
{
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
		
		if (value)
		{
			ss << field.name << "(" << value << ")";
		}
		
		return ss.str();
	}
	
	if (field.flag)
	{
		long value = get_field_value(code, field.bits);
		
		if (value)
		{
			return field.name;
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
			return field.name + "." + enum_eval;
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

long gpu_disassembler::check_field(const std::vector<uint32_t>& data, const gpu_asm::microcode_format_tuple& tuple, std::string field_name, std::string field_value)
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
	
	throw runtime_error("Field not found: " + field_name);
}

std::string gpu_disassembler::parse_literals(const std::vector<uint32_t>& data, int offset, int size)
{
	char buf[100];
	int pos = 0;
	
	for (int i = offset; i < offset+size; i++)
	{
		pos += sprintf(buf+pos, "\tLITERAL> 0x%.8X;\n", data[i]);
	}
	
	return buf;
}
