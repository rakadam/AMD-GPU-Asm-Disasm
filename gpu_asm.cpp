#include <map>
#include <iostream>
#include <stdexcept>
#include <assert.h>
#include <sstream>
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
			result += parse_tuple(data, match_tuple) + "\n";
			data.erase(data.begin(), data.begin() + match_size);
		}
	} while (match_num);
	
	if (data.size() != 0)
	{
		cerr << "Disassembling error at dword: " << orig_size - data.size() << endl;
		//hex_print(data[0]);
		throw runtime_error("sisassembling error");
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
	
	return (code & mask) >> shift;
}

std::string gpu_disassembler::get_enum_value(uint32_t code, gpu_asm::field field)
{
	assert(field.enum_name != "");
	long num_val = get_field_value(code, field.bits);
	
	for (auto i = field.vals.begin(); i != field.vals.end(); i++)
	{
		long start = min(i->value_bound.start, i->value_bound.stop);
		long stop = max(i->value_bound.start, i->value_bound.stop);
		
		if (num_val >= start or num_val <= stop)
		{
			string result = i->name;
			
			if (start != stop)
			{
				stringstream ss;
				ss << "(" << num_val - start << ")";
				result += ss.str();
			}
			
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
		result += "\t" + parse_microcode(data[i], asmdef.microcode_formats.at(tuple.tuple[i])) + ";\n";
	}
	
	return result;
}

std::string gpu_disassembler::parse_microcode(uint32_t code, const gpu_asm::microcode_format& format)
{
	std::string result;
	
	for (int i = 0; i < int(format.fields.size()); i++)
	{
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
		
// 		if (field.name == field.enum_name)
// 		{
// 			return enum_eval;
// 		}
		
		if (enum_eval != "")
		{
			return field.name + "." + enum_eval;
		}
		
		return "";
	}
	
	throw runtime_error("internal error: field has no valid type: " + field.name);
}

