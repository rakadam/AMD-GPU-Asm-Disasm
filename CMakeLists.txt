cmake_minimum_required(VERSION 2.6)

PROJECT(r800)

SET(CMAKE_CXX_FLAGS "-std=c++0x -O3 -g")

SET(r800_asm_lib_SRC
	gpu_asm.cpp
	gpu_asm_def.cpp
	asm_parser.cpp
	r800_def.hpp
)

ADD_EXECUTABLE(as_r800 ${r800_asm_lib_SRC} asm_cui.cpp)

ADD_EXECUTABLE(dis_r800 ${r800_asm_lib_SRC} main.cpp)
TARGET_LINK_LIBRARIES(dis_r800 elf)

ADD_EXECUTABLE(bin2cpp bin2cpp.cpp)

ADD_CUSTOM_COMMAND(OUTPUT r800_def.hpp
	COMMAND bin2cpp r800.def.ii r800_def r800_def.hpp
	DEPENDS bin2cpp r800.def.ii)
	
ADD_CUSTOM_COMMAND(OUTPUT r800.def.ii
	COMMAND cpp r800.def > r800.def.ii
	DEPENDS r800.def
 )
