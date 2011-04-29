#ifndef ASM_PARSER_HPP
#define ASM_PARSER_HPP
#include <vector>
#include <string>
#include "gpu_asm_def.hpp"

std::vector<gpu_asm::instruction> parse_asm_text(std::string text);

#endif
