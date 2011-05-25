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
#include <fstream>
#include <string>
#include <assert.h>
#include "gpu_asm.hpp"
#include "r800_def.hpp"

using namespace std;

int main(int argc, char *argv[])
{
	bool embed_c = false;
	
	assert(argc >= 2);
	
	if (argc == 3 and string(argv[2]) == "-ec")
	{
		embed_c = true;
	}
	
	gpu_asm::asm_definition asmdef(r800_def::str());
	gpu_assembler assembler(asmdef);
	
	string fname = argv[1];
	
	string fname_out, ext, name;
	
	if (embed_c)
	{
		ext = "_ec.h";
	}
	else
	{
		ext = ".bin";
	}
	
	if (fname.find(".asm") != string::npos)
	{
		name = fname.substr(0, fname.find(".asm"));
	}
	else
	{
		name = fname;
	}
	
	for (int i = 0; i < int(name.length()); i++)
	{
		if ((name[i] >= 'A' and name[i] <= 'Z') or
			 (name[i] >= 'a' and name[i] <= 'z') or
			 (name[i] >= '0' and name[i] <= '9'))
		{
		}
		else
		{
			name[i] = '_';
		}
	}
	
	fname_out = name + ext;
	
	ifstream f1(fname.c_str());
	
	string text = std::string(std::istreambuf_iterator<char>(f1), std::istreambuf_iterator<char>());
	
	auto binary = assembler.assemble(text);
	
	FILE *f2 = fopen(fname_out.c_str(), "w");
	
	if (embed_c)
	{
		fprintf(f2, "uint32_t %s_shader_binary[] = {\n", name.c_str());
		
		for (int i = 0; i < binary.size(); i++)
		{
			fprintf(f2, "0x%.8X", binary[i]);
			
			if (i != int(binary.size())-1)
			{
				fprintf(f2, ",");
			}
			
			if ((i+1)%4 == 0)
			{
				fprintf(f2, "\n");
			}
		}
		
		fprintf(f2, "};\n");
	}
	else
	{
		fwrite(&binary[0], sizeof(uint32_t), binary.size(), f2);
	}	
	
	fclose(f2);
	
	return 0;
}
