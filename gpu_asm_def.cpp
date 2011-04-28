#include <iostream>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/repository/include/qi_confix.hpp>


#include "gpu_asm_def.hpp"

using namespace boost::spirit::qi;
using namespace boost::spirit;

using namespace std;

// struct asm_definition
// {
// 	std::string arch_techname; //R800
// 	std::string arch_codename; //Evergreen
// 	
// 	std::map<std::string, microcode_format> microcode_formats; //formats by name
// 	std::map<std::string, microcode_format_tuple> microcode_format_tuples;
// 	
// 	asm_definition(std::string text); //parse definitions from text
// };


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

void print_info(boost::spirit::info const& what, bool detailed = false)
{
    using boost::spirit::basic_info_walker;

		cout << endl;
    printer pr(detailed);
    basic_info_walker<printer> walker(pr, what.tag, 0);
    boost::apply_visitor(walker, what.value);
}

namespace gpu_asm{

struct cur_attrib
{
	bound cur_bound;
	int cur_size;
	std::string cur_micro, cur_type, cur_enum, cur_elem, cur_field, cur_tuple;
};

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

struct assign_int
{
	int& i;
	
	assign_int(int& i) : i(i)
	{
	}
	
	void operator()(const int& i_, qi::unused_type, qi::unused_type) const
	{
		i = i_;
	}
};

struct print_str
{
	void operator()(const vector<char>& s, qi::unused_type, qi::unused_type) const
	{
		cout << std::string(s.begin(), s.end()) << endl;
	}
};

struct new_microcode_a
{
	asm_definition* asmdef;
	cur_attrib& attrib;
	
	new_microcode_a(asm_definition* asmdef, cur_attrib& attrib) 
		: asmdef(asmdef), attrib(attrib)
	{
	}
	
	void operator()(qi::unused_type, qi::unused_type, qi::unused_type) const
	{
	}
};

struct new_elem_a
{
	asm_definition* asmdef;
	cur_attrib& attrib;
	
	new_elem_a(asm_definition* asmdef, cur_attrib& attrib) 
		: asmdef(asmdef), attrib(attrib)
	{
	}
	
	void operator()(qi::unused_type, qi::unused_type, qi::unused_type) const
	{
	}
};

struct new_enum_a
{
	asm_definition* asmdef;
	cur_attrib& attrib;
	
	new_enum_a(asm_definition* asmdef, cur_attrib& attrib) 
		: asmdef(asmdef), attrib(attrib)
	{
	}
	
	void operator()(qi::unused_type, qi::unused_type, qi::unused_type) const
	{
	}
};

struct new_field_a
{
	asm_definition* asmdef;
	cur_attrib& attrib;
	
	new_field_a(asm_definition* asmdef, cur_attrib& attrib) 
		: asmdef(asmdef), attrib(attrib)
	{
	}
	
	void operator()(qi::unused_type, qi::unused_type, qi::unused_type) const
	{
	}
};

struct new_constraint_a
{
	asm_definition* asmdef;
	cur_attrib& attrib;
	
	new_constraint_a(asm_definition* asmdef, cur_attrib& attrib) 
		: asmdef(asmdef), attrib(attrib)
	{
	}
	
	void operator()(qi::unused_type, qi::unused_type, qi::unused_type) const
	{
	}
};

struct new_tuple_a
{
	asm_definition* asmdef;
	cur_attrib& attrib;
	
	new_tuple_a(asm_definition* asmdef, cur_attrib& attrib) 
		: asmdef(asmdef), attrib(attrib)
	{
	}
	
	void operator()(qi::unused_type, qi::unused_type, qi::unused_type) const
	{
	}
};

struct push_micro_a
{
	asm_definition* asmdef;
	cur_attrib& attrib;
	
	push_micro_a(asm_definition* asmdef, cur_attrib& attrib) 
		: asmdef(asmdef), attrib(attrib)
	{
	}
	
	void operator()(qi::unused_type, qi::unused_type, qi::unused_type) const
	{
	}
};

#define ref_ boost::phoenix::ref

std::string asm_definition::clear_comments(std::string text)
{
	using boost::phoenix::push_back;
	using boost::spirit::repository::confix;

	std::vector<char> result;
	
	auto comment_p1 = confix("/*", "*/")[*(char_ - "*/")];
	auto comment_p2 = confix("//", eol)[*(char_ - eol)];

	auto begin = text.begin();
	auto end = text.end();
	
	phrase_parse(begin, end, *char_[push_back(ref_(result), _1)], comment_p1 | comment_p2);
	
	return std::string(result.begin(), result.end());
}

#define name_(s) name[assign_str(attr.cur_ ## s)]
#define bstart_ int_[assign_int( attr.cur_bound.start )]
#define bstop_ int_[assign_int( attr.cur_bound.stop )]
#define bpos_ int_[assign_int( attr.cur_bound.start )][assign_int( attr.cur_bound.stop )]

#define new_microcode new_microcode_a(this, attr)
#define new_elem new_elem_a(this, attr)
#define new_enum new_enum_a(this, attr)
#define new_field new_field_a(this, attr)
#define new_constraint new_constraint_a(this, attr)
#define new_tuple new_tuple_a(this, attr)
#define push_micro push_micro_a(this, attr)


asm_definition::asm_definition(std::string text)
{
	text = clear_comments(text);
	cur_attrib attr;
	
	std::map<std::string, std::set<enum_val> > enums;
	
	auto name = lexeme[*(alnum >> *lit("_"))];
	
	auto header = "architecture" > name[assign_str(arch_techname)] > name[assign_str(arch_codename)] > ';';
	auto bbound_p = ('(' >> bpos_ >> ')') | ('(' >> bstart_ >> ':' >> bstop_ >> ')'); 
	auto bound_p = (bstart_ >> ':' >> bstop_) | bpos_; 
	auto size_p = ('(' > int_[assign_int(attr.cur_size)] > ')');
	
	auto debug = lexeme[(*char_)[print_str()]];
	auto enum_elem = !lit("end") > name_(elem)[ref_(attr.cur_bound.start) = -1] > -bound_p > lit(';')[new_elem];
	auto enum_def = "enum" > size_p > name_(enum) > lit(':')[new_enum] > *enum_elem > "end" > "enum" > ';';
	auto field = "field" > name_(field) > bbound_p > name_(type) > lit(';')[new_field];
	auto microcode_def = "microcode" > name_(micro) >  size_p > lit(':')[new_microcode]  > *(enum_def | field)  > "end" > "microcode" > ';';
	auto microcode_use = "microcode" > name_(micro) > lit(';')[push_micro];
	auto constraint_def = !lit("end") > name_(micro) > '.' > name_(field) > "==" > name_(elem) > lit(';')[new_constraint];
	auto constraints_def = "constraints" >> lit(':') > *constraint_def > "end" > "constraints" > ';';
	auto tuple_def = "tuple" > name_(tuple) >  size_p > lit(':')[new_tuple]  > *microcode_use > -constraints_def > "end" > "tuple" > ';';
	
	auto begin = text.begin();
	auto end = text.end();
	
	int linenum = 0;
	
	for (int i = 0; i < int(text.length()); i++)
		if (text[i] == '\n')
			linenum++;
	
	try{
		phrase_parse(begin, end, eps > header > *(microcode_def | tuple_def) > "end" > ';' > eoi, space);
	}
	catch (expectation_failure<decltype(begin)> const& x)
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
	}
}



}