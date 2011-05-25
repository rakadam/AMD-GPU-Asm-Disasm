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

#include <iostream>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/repository/include/qi_confix.hpp>
#include "asm_parser.hpp"

using namespace std;

using namespace boost::spirit;

static std::string clear_comments(std::string text)
{
	using boost::spirit::qi::phrase_parse;
	using boost::spirit::qi::char_;
	using boost::spirit::qi::eol;
	using boost::phoenix::push_back;
	using boost::phoenix::ref;
	using boost::spirit::repository::confix;

	std::vector<char> result;
	
	auto comment_p1 = confix("/*", "*/")[*(char_ - "*/")];
	auto comment_p2 = confix("//", eol)[*(char_ - eol)];
	auto comment_p3 = confix("(*", "*)")[*(char_ - "*)")];

	auto begin = text.begin();
	auto end = text.end();
	
	phrase_parse(begin, end, *char_[push_back(boost::phoenix::ref(result), _1)], comment_p1 | comment_p2 | comment_p3);
	
	return std::string(result.begin(), result.end());
}


struct printer
{
		bool detailed;
		
    typedef boost::spirit::utf8_string string;

		printer(bool detailed = false) : detailed(detailed) {}
		
    void element(string const& tag, string const& value, int depth) const
    {
			for (int i = 0; i < (8+depth*4); ++i) // indent to depth
			{
				std::cout << ' ';
			}
				
			if (detailed)
			{
				
				std::cout << "tag: " << tag;
				
				if (value != "")
				{
					std::cout << ", value: \"" << value << "\"";
				}
				
				std::cout << std::endl;
			}
			else
			{
				if (value != "")
				{
					cout << value << endl;
				}
				else
				{
					cout << tag << endl;
				}
			}
    }
};

static void print_info(boost::spirit::info const& what, bool detailed = false)
{
    using boost::spirit::basic_info_walker;

		cout << endl;
    printer pr(detailed);
    basic_info_walker<printer> walker(pr, what.tag, 0);
    boost::apply_visitor(walker, what.value);
}

struct assign_str
{
	std::string& str;
	
	assign_str(std::string& s) : str(s)
	{
	}
	
	void operator()(const vector<char>& s, qi::unused_type, qi::unused_type) const
	{
		str = std::string(s.begin(), s.end());
	}
};

struct new_instruction
{
	vector<gpu_asm::instruction>& istream;
	string& label;
	
	new_instruction(vector<gpu_asm::instruction>& istream, string& label) : istream(istream) , label(label)
	{
	}
	
	void operator()(const vector<char>& s, qi::unused_type, qi::unused_type) const
	{
		string name(s.begin(), s.end());
// 		cout << name << endl;
		istream.push_back(gpu_asm::instruction());
		istream.back().name = name;
		istream.back().label = label;
		label = "";
	}
};

// struct new_microcode
// {
// 	vector<gpu_asm::instruction>& istream;
// 
// 	new_microcode(vector<gpu_asm::instruction>& istream) : istream(istream)
// 	{
// 	}
// 	
// 	void operator()(const boost::optional<vector<char>>& s_, qi::unused_type, qi::unused_type) const
// 	{
// 		auto s = s_.get_value_or(vector<char>(0));
// 		string name(s.begin(), s.end());
// // 		cout << name << endl;
// 		istream.back().microcodes.push_back(gpu_asm::microcode());
// 		istream.back().microcodes.back().name = name;
// 	}
// };

struct new_field
{
	vector<gpu_asm::instruction>& istream;

	new_field(vector<gpu_asm::instruction>& istream) : istream(istream)
	{
	}
	
	void operator()(const vector<char>& s, qi::unused_type, qi::unused_type) const
	{
		string name(s.begin(), s.end());
		
		istream.back().fields.push_back(gpu_asm::microcode_field());
		istream.back().fields.back().name = name;
	}
};

struct set_enum
{
	vector<gpu_asm::instruction>& istream;

	set_enum(vector<gpu_asm::instruction>& istream) : istream(istream)
	{
	}
	
	void operator()(const vector<char>& s, qi::unused_type, qi::unused_type) const
	{
		string name(s.begin(), s.end());
// 		cout << name << endl;
		istream.back().fields.back().enum_elem = name;
	}
};

struct set_field_label
{
	vector<gpu_asm::instruction>& istream;

	set_field_label(vector<gpu_asm::instruction>& istream) : istream(istream)
	{
	}
	
	void operator()(const vector<char>& s, qi::unused_type, qi::unused_type) const
	{
		string name(s.begin(), s.end());
// 		cout << name << endl;
		istream.back().fields.back().label = name;
	}
};

struct set_num
{
	vector<gpu_asm::instruction>& istream;

	set_num(vector<gpu_asm::instruction>& istream) : istream(istream)
	{
	}
	
	void operator()(int i, qi::unused_type, qi::unused_type) const
	{
// 		cout << i << endl;
		istream.back().fields.back().offset = i;
		istream.back().fields.back().offset_is_set = true;
	}
};


#define new_instruction new_instruction(istream, future_label)
#define new_field new_field(istream)
#define set_enum set_enum(istream)
#define set_field_label set_field_label(istream)
#define set_num set_num(istream)

std::vector<gpu_asm::instruction> parse_asm_text(std::string text)
{
	using boost::spirit::qi::phrase_parse;
	using boost::spirit::qi::eps;
	using boost::spirit::qi::lexeme;
	using boost::spirit::qi::char_;
	using boost::spirit::qi::alnum;
	using boost::spirit::qi::int_;
	using boost::spirit::ascii::space;
	using boost::spirit::qi::expectation_failure;
	
	vector<gpu_asm::instruction> istream;
	string future_label;
	
	text = clear_comments(text);
	
	auto name = lexeme[+(alnum | char_('_'))];
	auto num = '(' > (int_[set_num] | (lit('@') > name[set_field_label])) > ')';
	auto field = name[new_field] > -('.' > name[set_enum]) > -(num);
	auto microcode = !(name >> ':') >> !lit('@') >> !(lit("end") > ";") >> *field > ';';
	auto instruction = -(lit('@') > name[assign_str(future_label)]) >> !(lit("end") > ";") >> (name >> ':')[new_instruction] > *microcode;
	
	auto begin = text.begin();
	auto end = text.end();
	
	int linenum = 0;
	
	for (int i = 0; i < int(text.length()); i++)
		if (text[i] == '\n')
			linenum++;
	
	try{
		phrase_parse(begin, end, eps > *instruction > "end" > ";", space);
	} catch (expectation_failure<decltype(begin)> const& x)
	{
		std::cout << "expected: "; print_info(x.what_);
		std::string got(x.first, x.last);
		
		int gotlinenum = 0;
		
		for (int i = 0; i < int(got.length()); i++)
			if (got[i] == '\n')
				gotlinenum++;

		int gotsize = got.size();
		
		if (got.find("\n") != std::string::npos)
		{
			got.resize(got.find("\n"));
		}
		
		std::cout << "got: \"" << got << "\" at line: #" << linenum - gotlinenum +1 << std::endl;
		
		int pos = 0;
		int cpos = int(text.length()) - gotsize;
		
		for (int i = int(text.length()) - gotsize; i >= 0; i--)
		{
			if (text[i] == '\n')
			{
				pos = i+1;
				break;
			}
		}
		
		int pos2 = text.length();
		
		for (int i = pos; i < int(text.length()); i++)
		{
			if (text[i] == '\n')
			{
				pos2 = i;
				break;
			}
		}
		
		std::string line(text.begin()+pos, text.begin()+pos2);
		
		for (int i = 0; i < int(line.length()); i++)
		{
			if (line[i] == '\t')
			{
				line[i] = ' ';
			} 
		}
		
		std::cout << line << endl;
		
		for (int i = 0; i < cpos-pos; i++)
		{
			cout << " ";
		}
		
		cout << "^" << endl;
		
		throw std::runtime_error("Syntax error");
	}
	
	return istream;
}


void parse_field_value(std::string text, long offset, std::string& name)
{
	using boost::spirit::qi::phrase_parse;
	using boost::spirit::qi::eps;
	using boost::spirit::qi::lexeme;
	using boost::spirit::qi::char_;
	using boost::spirit::qi::alnum;
	using boost::spirit::qi::int_;
	using boost::spirit::ascii::space;
	using boost::spirit::qi::expectation_failure;
	
	if (text == "")
	{
		offset = 0;
		name = "";
		return;
	}
	
	
	auto num = int_[boost::phoenix::ref(offset) = _1];
	auto name_p = lexeme[+(alnum | char_('_'))][assign_str(name)];
	auto offset_p = '(' > num > ')';
	auto value = num | (name_p >> -offset_p);
	
	auto begin = text.begin();
	auto end = text.end();

	try{
		phrase_parse(begin, end, eps > value, space);
	} catch (expectation_failure<decltype(begin)> const& x)
	{
		cout << "expected: "; print_info(x.what_);
		string got(x.first, x.last);
		cout << "got: " << got << endl;
	}
}
