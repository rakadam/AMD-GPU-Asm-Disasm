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
	
	auto name = lexeme[*(alnum)];
	
	auto header = "architecture" >> name[assign_str(arch_techname)] >> name[assign_str(arch_codename)] >> ';';
	auto bbound_p = ('(' >> int_ >> ')') | ('(' >> int_ >> ':' >> int_ >> ')'); 
	auto bound_p = (int_ >> ':' >> int_) | int_; 
	auto size_p = ('(' >> int_ >> ')');
	
	auto enum_elem = name >> bound_p >> ';';
	auto enum_def = "enum" >> size_p >> name >> ':' >> *enum_elem >> "end" >> "enum" >> ';';
	auto field = "field" >> name >> bbound_p >> name >> ';';
	auto microcode_def = "microcode" >> name >> bbound_p >> ':' >> *(enum_def | field) >> "end" >> "microcode" >> ';';
/*	auto tuple_def = ;
	auto microcode_use = ;
	auto constraints_def = ;
	auto constraint_def = ;*/
	
	auto begin = text.begin();
	auto end = text.end();
	phrase_parse(begin, end, header >> microcode_def, space);
	
	
	cout << arch_techname << "-" << arch_codename << endl;
	
	
 	cout << std::string(begin, end) << endl;
}



}