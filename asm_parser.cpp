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


struct new_instruction
{
	vector<gpu_asm::instruction>& istream;

	new_instruction(vector<gpu_asm::instruction>& istream) : istream(istream)
	{
	}
	
	void operator()(const vector<char>& s, qi::unused_type, qi::unused_type) const
	{
		string name(s.begin(), s.end());
// 		cout << name << endl;
		istream.push_back(gpu_asm::instruction());
		istream.back().name = name;
	}
};

struct new_microcode
{
	vector<gpu_asm::instruction>& istream;

	new_microcode(vector<gpu_asm::instruction>& istream) : istream(istream)
	{
	}
	
	void operator()(const vector<char>& s, qi::unused_type, qi::unused_type) const
	{
		string name(s.begin(), s.end());
// 		cout << name << endl;
		istream.back().microcodes.push_back(gpu_asm::microcode());
		istream.back().microcodes.back().name = name;
	}
};

struct new_field
{
	vector<gpu_asm::instruction>& istream;

	new_field(vector<gpu_asm::instruction>& istream) : istream(istream)
	{
	}
	
	void operator()(const vector<char>& s, qi::unused_type, qi::unused_type) const
	{
		string name(s.begin(), s.end());
		
// 		cout << name << endl;
		istream.back().microcodes.back().fields.push_back(gpu_asm::microcode_field());
		istream.back().microcodes.back().fields.back().name = name;
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
		istream.back().microcodes.back().fields.back().enum_elem = name;
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
		istream.back().microcodes.back().fields.back().offset = i;
	}
};


#define new_instruction new_instruction(istream)
#define new_microcode new_microcode(istream)
#define new_field new_field(istream)
#define set_enum set_enum(istream)
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
	
	text = clear_comments(text);
	
	auto name = lexeme[+(alnum | char_('_'))];
	auto num = '(' > int_ > ')';
	auto field = name[new_field] > -('.' > name[set_enum]) > -(num[set_num]);
	auto microcode = (name >> '>')[new_microcode] > *field > ';';
	auto instruction = (name >> ':')[new_instruction] > *microcode;
	
	auto begin = text.begin();
	auto end = text.end();
	
	try{
		phrase_parse(begin, end, eps > *instruction > "end" > ";", space);
	} catch (expectation_failure<decltype(begin)> const& x)
	{
		cout << "expected: "; print_info(x.what_);
		string got(x.first, x.last);
		cout << "got: " << got << endl;
	}
	
	return istream;
}
