#include <iostream>
#include <stdexcept>
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
	bound cur_bound, cur_bound2;
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
		if (asmdef->microcode_formats.count(attrib.cur_micro))
		{
			throw std::runtime_error("Redefinition of microcode format: " + attrib.cur_micro);
		}
		
		asmdef->microcode_formats[attrib.cur_micro].size_in_bits = attrib.cur_size;
		asmdef->microcode_formats[attrib.cur_micro].name = attrib.cur_micro;
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
		enum_val val;
		val.name = attrib.cur_elem;
		val.value_bound = attrib.cur_bound;
		
		if (attrib.cur_bound.start == -1)
		{
			val.value_bound.start = asmdef->microcode_formats.at(attrib.cur_micro).enums.at(attrib.cur_enum).size();
			val.value_bound.stop = asmdef->microcode_formats.at(attrib.cur_micro).enums.at(attrib.cur_enum).size();
		}
		
		if (asmdef->microcode_formats.at(attrib.cur_micro).enums.at(attrib.cur_enum).count(val))
		{
			throw std::runtime_error("Redefinition of element: " + attrib.cur_micro + "." + attrib.cur_enum + "." + attrib.cur_elem);
		}
		
		asmdef->microcode_formats.at(attrib.cur_micro).enums.at(attrib.cur_enum).insert(val);

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
		if (asmdef->microcode_formats.at(attrib.cur_micro).enums.count(attrib.cur_enum))
		{
			throw std::runtime_error("Redefinition of enum: " + attrib.cur_enum + " in microcode format: " + attrib.cur_micro);
		}
		
		asmdef->microcode_formats.at(attrib.cur_micro).enums[attrib.cur_enum];
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
		bool found = false;
		auto fields = asmdef->microcode_formats.at(attrib.cur_micro).fields;
		
		for (int i = 0; i < int(fields.size()); i++)
		{
			if (fields[i].name == attrib.cur_field)
			{
				found = true;
			}
		}
		
		if (found)
		{
			throw std::runtime_error("Redefinition of field: " + attrib.cur_micro + "." + attrib.cur_field);
		}
		
		field ff;
		
		ff.name = attrib.cur_field;
		ff.bits = attrib.cur_bound;
		
		if (attrib.cur_type == "INT")
		{
			ff.numeric = true;
			ff.flag = false;
			ff.numeric_bounds = attrib.cur_bound2;
			
			if (attrib.cur_bound2.start == -1)
			{
				ff.numeric_bounds.stop = 0;
				ff.numeric_bounds.start = int(pow(2, abs(ff.bits.start-ff.bits.stop)+1));
			}
		}
		else if (attrib.cur_type == "BOOL")
		{
			ff.numeric = false;
			ff.flag = true;
		}
		else
		{
			if (asmdef->microcode_formats.at(attrib.cur_micro).enums.count(attrib.cur_type) == 0)
			{
				throw std::runtime_error("Undefined type: " + attrib.cur_micro + "." + attrib.cur_type);
			}
			
			ff.vals = asmdef->microcode_formats.at(attrib.cur_micro).enums.at(attrib.cur_type);
			ff.enum_name = attrib.cur_type;
		}
		
		asmdef->microcode_formats.at(attrib.cur_micro).fields.push_back(ff);
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
		if (asmdef->microcode_formats.count(attrib.cur_micro))
		{
		}
		
		bool found = false;
		auto fields = asmdef->microcode_formats.at(attrib.cur_micro).fields;
		field ff;
		
		for (int i = 0; i < int(fields.size()); i++)
		{
			if (fields[i].name == attrib.cur_field)
			{
				found = true;
				ff = fields[i];
			}
		}
		
		if (!found)
		{
			throw std::runtime_error("Undefined field: " + attrib.cur_micro + "." + attrib.cur_field);
		}
		
		enum_val val;
		val.name = attrib.cur_elem;
		
		if (ff.vals.size() == 0)
		{
			throw std::runtime_error("Field is not a valid enum: " + attrib.cur_micro + "." + attrib.cur_field);
		}
		
		if (!ff.vals.count(val))
		{
			throw std::runtime_error("Undefined element of an enum: " + attrib.cur_micro + "." + attrib.cur_field + "." + ff.enum_name +  "." + attrib.cur_elem);
		}
		
		asmdef->microcode_format_tuples.at(attrib.cur_tuple).constraints[make_pair(attrib.cur_micro, attrib.cur_field)] = attrib.cur_elem;
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
		if (asmdef->microcode_format_tuples.count(attrib.cur_tuple))
		{
			throw std::runtime_error("Redefinition of microcode format tuple: " + attrib.cur_tuple);
		}
		
		asmdef->microcode_format_tuples[attrib.cur_tuple].size_in_bits = attrib.cur_size;
		asmdef->microcode_format_tuples[attrib.cur_tuple].name = attrib.cur_tuple;
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
		if (!asmdef->microcode_formats.count(attrib.cur_micro))
		{
			throw std::runtime_error("Undefined microcode format: " + attrib.cur_tuple + "." + attrib.cur_micro);
		}
		
		asmdef->microcode_format_tuples[attrib.cur_tuple].tuple.push_back(attrib.cur_micro);
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

#define name_(s) name[assign_str(attr.cur_ ## s)][ref_(attr.cur_bound.start) = -1][ref_(attr.cur_bound2.start) = -1]
#define bstart_ int_[assign_int( attr.cur_bound.start )]
#define bstop_ int_[assign_int( attr.cur_bound.stop )]
#define bpos_ int_[assign_int( attr.cur_bound.start )][assign_int( attr.cur_bound.stop )]
#define bstart2_ int_[assign_int( attr.cur_bound2.start )]
#define bstop2_ int_[assign_int( attr.cur_bound2.stop )]

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
	
	auto name = lexeme[*(alnum | char_('_'))];
	auto header = "architecture" > name[assign_str(arch_techname)] > name[assign_str(arch_codename)] > ';';
	auto bbound_p = ('(' >> bpos_ >> ')') | ('(' >> bstart_ >> ':' >> bstop_ >> ')'); 
	auto bbound2_p = '(' >> bstart2_ >> ':' >> bstop2_ >> ')'; 
	auto bound_p = (bstart_ >> ':' >> bstop_) | bpos_; 
	auto size_p = ('(' > int_[assign_int(attr.cur_size)] > ')');
	
	auto debug = lexeme[(*char_)[print_str()]];
	auto enum_elem = !lit("end") > name_(elem) > -bound_p > lit(';')[new_elem];
	auto enum_def = "enum" > size_p > name_(enum) > lit(':')[new_enum] > *enum_elem > "end" > "enum" > ';';
	auto field = "field" > name_(field) > bbound_p > name_(type) > -bbound2_p > lit(';')[new_field];
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