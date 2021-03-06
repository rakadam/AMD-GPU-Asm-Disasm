/*
 * Copyright 2011 StreamNovation Ltd. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY StreamNovation Ltd. ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL StreamNovation Ltd. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR                                                                                                                    
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON                                                                                                                    
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING                                                                                                                          
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF                                                                                                                        
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                                                                                                                                                  
 *                                                                                                                                                                                                             
 * The views and conclusions contained in the software and documentation are those of the                                                                                                                      
 * authors and should not be interpreted as representing official policies, either expressed                                                                                                                   
 * or implied, of StreamNovation Ltd.                                                                                                                                                                          
 *                                                                                                                                                                                                             
 *                                                                                                                                                                                                             
 * Author(s):                                                                                                                                                                                                  
 *          Adam Rak <adam.rak@streamnovation.com>
 *    
 *    
 *    
 */

#ifndef GPU_ASM_DEF_HPP
#define GPU_ASM_DEF_HPP
#include <string>
#include <vector>
#include <set>
#include <map>

namespace gpu_asm
{
	
struct bound
{
	bound() : start(-1), stop(-1) {}
	
	int start;
	int stop;
};

struct enum_val
{
	enum_val() : name(""), value_bound(), default_option(false) {}
	
	std::string name;
	bound value_bound; //start == stop : simple enumeration; stop > start: name can have an offset
	bool default_option; //and value_bound should be 0 too!
	
	bool operator == (const enum_val& b) const
	{
		return name == b.name;
	}
	
	bool operator > (const enum_val& b) const
	{
		return name > b.name;
	}
	
	bool operator < (const enum_val& b) const
	{
		return name < b.name;
	}
};

struct field
{
	field() : name(""), bits(), numeric(false), numeric_bounds(), flag(false), vals(), enum_name() {}
	
	std::string name;
	
	bound bits;
	
	//numeric constant
	bool numeric;
	bound numeric_bounds;
	
	//1bit flag
	bool flag;
	
	//enum
	std::set<enum_val> vals; 
	std::string enum_name;
};

struct microcode_format
{
	int size_in_bits;
	std::string name;
	std::vector<field> fields;
	std::map<std::string, std::set<enum_val> > enums;
};

struct microcode_format_tuple
{
	std::string name;
	int size_in_bits;
	std::vector<std::string> tuple;
	std::map<std::pair<std::string, std::string>, std::string> constraints; //KEY,VALUE pairs of field constraints in this format tuple
	std::set<std::string> options;
};

struct asm_definition
{
	std::string arch_techname; //R800
	std::string arch_codename; //Evergreen
	
	std::map<std::string, microcode_format> microcode_formats; //formats by name
	std::map<std::string, microcode_format_tuple> microcode_format_tuples;
	
	asm_definition(std::string text); //parse definitions from text
private:
	std::string clear_comments(std::string text);
	bool check(std::ostream&);
	bool check_tuple(std::ostream&, microcode_format_tuple);
	bool check_format(std::ostream&, microcode_format);
};


//for assembly representation

struct microcode_field
{
	microcode_field() : name(""), offset_is_set(false), offset(0), enum_elem("") {}
	microcode_field(std::string name, int index) : name(name), offset_is_set(false), offset(index), enum_elem("") {}
	std::string name;
	bool offset_is_set;
	long offset; //for INT it is the actual value, for enum it is offset, nothing for BOOL
	std::string enum_elem; //empty for BOOL and INT
	std::string label; //if it uses a label
};

struct instruction
{
	std::string name;
	std::string label; //if it has a label
	std::vector<microcode_field> fields;
};

}

#endif
