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

struct print_str
{
	void operator()(const vector<char>& s, qi::unused_type, qi::unused_type) const
	{
		cout << std::string(s.begin(), s.end()) << endl;
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

asm_definition::asm_definition(std::string text)
{
	text = clear_comments(text);
	bound cur_bound;
	std::map<std::string, std::set<enum_val> > enums;
	
	auto name = lexeme[*(alnum >> *lit("_"))];
	
	auto header = "architecture" > name[assign_str(arch_techname)] > name[assign_str(arch_codename)] > ';';
	auto bbound_p = ('(' >> int_ >> ')') | ('(' >> int_ >> ':' >> int_ >> ')'); 
	auto bound_p = (int_ >> ':' >> int_) | int_; 
	auto size_p = ('(' > int_ > ')');
	
	auto debug = lexeme[(*char_)[print_str()]];
	auto enum_elem = !lit("end") > name > -bound_p > ';';
	auto enum_def = "enum" > size_p > name > ':' > *enum_elem > "end" > "enum" > ';';
	auto field = "field" > name > bbound_p > name >> ';';
	auto microcode_def = "microcode" > name >  bbound_p > ':'  > *(enum_def | field)  > "end" > "microcode" > ';';
	auto microcode_use = "microcode" > name >> ';';
	auto constraint_def = !lit("end") > name > '.' > name > "==" > name > ';';
	auto constraints_def = "constraints" >> lit(':') > *constraint_def > "end" > "constraints" > ';';
	auto tuple_def = "tuple" > name >  bbound_p > ':'  > *microcode_use > -constraints_def > "end" > "tuple" > ';';
	
	auto begin = text.begin();
	auto end = text.end();
	
	int linenum = 0;
	
	for (int i = 0; i < text.length(); i++)
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
		
		for (int i = 0; i < got.length(); i++)
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
		
		for (int i = pos; i < text.length(); i++)
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