#include <iostream>
#include <boost/spirit/include/classic.hpp>
#include "gpu_asm_def.hpp"

using namespace boost::spirit::classic;

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
	
string asm_definition::clear_comments(string text)
{
	return text;
}

asm_definition::asm_definition(std::string text)
{
	cout << clear_comments(text) << endl;
}



}