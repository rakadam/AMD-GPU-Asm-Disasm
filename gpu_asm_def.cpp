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

namespace gpu_asm{

struct cur_attrib
{
	bound cur_bound, cur_bound2;
	int cur_size;
	std::string cur_micro, cur_type, cur_enum, cur_elem, cur_field, cur_tuple;
	bool cur_default_val;
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
		val.default_option = attrib.cur_default_val;
		
		if (attrib.cur_bound.start == -1)
		{
			val.value_bound.start = asmdef->microcode_formats.at(attrib.cur_micro).enums.at(attrib.cur_enum).size();
			val.value_bound.stop = asmdef->microcode_formats.at(attrib.cur_micro).enums.at(attrib.cur_enum).size();
		}
		
		if (val.default_option and (val.value_bound.start != 0 or val.value_bound.stop != 0))
		{
			throw std::runtime_error("Default options should evaluate to zero " + attrib.cur_micro + "." + attrib.cur_enum + "." + attrib.cur_elem);
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
		
// 		cout << attrib.cur_field << " : " << ff.bits.start << " " << ff.bits.stop << endl;
		
		if (attrib.cur_type == "INT")
		{
			ff.numeric = true;
			ff.flag = false;
			ff.numeric_bounds = attrib.cur_bound2;
			
			if (attrib.cur_bound2.start == -1)
			{
				ff.numeric_bounds.stop = 0;
				ff.numeric_bounds.start = int(pow(2, abs(ff.bits.start-ff.bits.stop)+1))-1;
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



struct set_default_elem_a
{
	asm_definition* asmdef;
	cur_attrib& attrib;
	
	set_default_elem_a(asm_definition* asmdef, cur_attrib& attrib) 
		: asmdef(asmdef), attrib(attrib)
	{
	}
	
	void operator()(qi::unused_type, qi::unused_type, qi::unused_type) const
	{
		attrib.cur_default_val = true;
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
	auto comment_p3 = confix("(*", "*)")[*(char_ - "*)")];
	auto comment_p4 = confix("#", eol)[*(char_ - eol)];

	auto begin = text.begin();
	auto end = text.end();
	
	phrase_parse(begin, end, *char_[push_back(ref_(result), _1)], comment_p1 | comment_p2 | comment_p3 | comment_p4);
	
	return std::string(result.begin(), result.end());
}

#define name_(s) name[assign_str(attr.cur_ ## s)]
#define name_c(s) name[assign_str(attr.cur_ ## s)][ref_(attr.cur_bound.start) = -1][ref_(attr.cur_bound2.start) = -1][ref_(attr.cur_default_val) = false]
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
#define set_default_elem set_default_elem_a(this, attr)

asm_definition::asm_definition(std::string text)
{
	text = clear_comments(text);
	cur_attrib attr;
	
	std::map<std::string, std::set<enum_val> > enums;
	
	auto name = lexeme[+(alnum | char_('_'))];
	auto header = "architecture" > name[assign_str(arch_techname)] > name[assign_str(arch_codename)] > ';';
	auto bbound_p = ('(' >> bpos_ >> ')') | ('(' >> bstart_ >> ':' >> bstop_ >> ')'); 
	auto bbound2_p = '(' >> bstart2_ >> ':' >> bstop2_ >> ')'; 
	auto bound_p = (bstart_ >> ':' >> bstop_) | bpos_; 
	auto size_p = ('(' > int_[assign_int(attr.cur_size)] > ')');
	
	auto debug = lexeme[(*char_)[print_str()]];
	auto enum_elem = !lit("end") > name_c(elem) > -bound_p > -lit("default")[set_default_elem] > lit(';')[new_elem];
	auto enum_def = "enum" > size_p > name_(enum) > lit(':')[new_enum] > *enum_elem > "end" > "enum" > ';';
	auto field = "field" > name_c(field) > bbound_p > name_(type) > -bbound2_p  > lit(';')[new_field];
	auto microcode_def = "microcode" > name_(micro) >  size_p > lit(':')[new_microcode]  > *(enum_def | field)  > "end" > "microcode" > ';';
	auto microcode_use = "microcode" > name_(micro) > lit(';')[push_micro];
	auto constraint_def = !lit("end") > name_(micro) > '.' > name_(field) > "==" > name_(elem) > lit(';')[new_constraint];
	auto constraints_def = "constraints" >> lit(':') > *constraint_def > "end" > "constraints" > ';';
	auto tuple_def = "tuple" > name_c(tuple) >  size_p > lit(':')[new_tuple]  > *microcode_use > -constraints_def > "end" > "tuple" > ';';
	
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
		
		throw std::runtime_error("Syntax error");
	}
	
	if (!check(cerr))
	{
		throw std::runtime_error("Semantic error");
	}
}

bool asm_definition::check(std::ostream& o)
{
	for (auto i = microcode_format_tuples.begin(); i != microcode_format_tuples.end(); i++)
	{
		if (!check_tuple(o, i->second))
			return false;
	}
	
	for (auto i = microcode_formats.begin(); i != microcode_formats.end(); i++)
	{
		if (!check_format(o, i->second))
			return false;
	}
	
	return true;
}

bool asm_definition::check_tuple(std::ostream& o, microcode_format_tuple t)
{
	int true_size = 0;
	std::set<std::string> code_names;
	
	for (int i = 0; i < int(t.tuple.size()); i++)
	{
		code_names.insert(t.tuple[i]);
		true_size += microcode_formats.at(t.tuple[i]).size_in_bits;
		assert(microcode_formats.at(t.tuple[i]).name == t.tuple[i]);
	}
	
	for (auto i = t.constraints.begin(); i != t.constraints.end(); i++)
	{
		if (!code_names.count(i->first.first))
		{
			o << "microcode format is not in tuple: " << t.name << "." << i->first.first << endl;
			return false;
		}
	}
	
	if (true_size > t.size_in_bits)
	{
		o << "Tuple " << t.name << " is bigger (" << true_size << ")" << "than the nominal size: " << t.size_in_bits << endl;
		return false;
	}
	
	if (true_size < t.size_in_bits)
	{
		o << "Tuple " << t.name << " is smaller (" << true_size << ")" << "than the nominal size: " << t.size_in_bits << endl;
	}
	
	return true;
}

bool asm_definition::check_format(std::ostream& o, microcode_format f)
{
	int true_size = 0;
	std::map<int, bool> bit_usage;
	
	for (int i = 0; i < int(f.fields.size()); i++)
	{
		int cur_size = abs(f.fields[i].bits.start-f.fields[i].bits.stop)+1;
		true_size += cur_size;
		
		if (f.fields[i].numeric)
		{
			long max_val = std::max(f.fields[i].numeric_bounds.start, f.fields[i].numeric_bounds.stop);
			
			long c_max_val = pow(2, cur_size);
			
			if (max_val >= c_max_val)
			{
				o << "Representation overflow at field: " << f.name << "." << f.fields[i].name << " " << max_val << " > " << c_max_val-1 << "(" << cur_size << " bits)" << endl;
				return false;
			}
		}
		else if (f.fields[i].vals.size())
		{
			for (auto j = f.fields[i].vals.begin(); j != f.fields[i].vals.end(); j++)
			{
				long max_val = std::max(j->value_bound.start, j->value_bound.stop);
				
				long c_max_val = pow(2, cur_size);
				
				if (max_val >= c_max_val)
				{
					o << "Representation overflow at field: " << f.name << "." << f.fields[i].name << " " << max_val << " > " << c_max_val-1 << "(" << cur_size << " bits)" << " in elem: " << f.fields[i].enum_name << "." << j->name << endl;
					return false;
				}
			}
		}
		else if (f.fields[i].flag)
		{
			if (cur_size != 1)
			{
				o << "Field " << f.name << "." << f.fields[i].name << " is a flag, but it occupies " << cur_size << " bits instead of one" << endl;
				return false;
			}
		}
		else
		{
			o << "Field " << f.name << "." << f.fields[i].name << " has no valid type" << endl;
			return false;
		}
		
		long begin = std::min(f.fields[i].bits.start, f.fields[i].bits.stop);
		long end = std::max(f.fields[i].bits.start, f.fields[i].bits.stop);
		
		if (std::max(begin, end) >= f.size_in_bits)
		{
			o << "Field " << f.name << "." << f.fields[i].name << " uses bits outside the microcode format range: " << end << " > " << f.size_in_bits-1 << endl;
			return false;
		}
		
		for (int j = begin; j <= end; j++)
		{
			if (bit_usage[j])
			{
				o << "Bit " << j << " was reused at: " << f.name << "." << f.fields[i].name << endl;
				true_size--;
			}
			
			bit_usage[j] = true;
		}
	}
	
	return true;
}


}