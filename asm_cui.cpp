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
#include <sstream>
#include <fstream>
#include <string>
#include <assert.h>
#include <fcntl.h>
#include <gelf.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "gpu_asm.hpp"
#include "r800_def.hpp"

using namespace std;

void to_file(char *data, size_t size, string name)
{
  FILE *f = fopen(name.c_str(), "w");
  
  fwrite(data, 1, size, f);
  
  fclose(f);
}

int main(int argc, char *argv[])
{
	bool embed_c = false;
	bool amd_sdk_elf = false;
  
	assert(argc >= 2);
	
	if (argc == 3 and string(argv[2]) == "-ec")
	{
		embed_c = true;
	}

  if (argc == 4 and string(argv[2]) == "-amd")
  {
    amd_sdk_elf = true;
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
	
	FILE *f2 = NULL;
  
  if (!amd_sdk_elf)
  {
    f2 = fopen(fname_out.c_str(), "w");
  }
  
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
	else if (amd_sdk_elf)
  {
    assert(elf_version(EV_CURRENT) != EV_NONE);

    int fd1 = open(argv[3], O_RDWR);
    
    if (fd1 < 1)
    {
      cerr << argv[3] << " cannot be found" << endl;
      return 1;
    }
    
    Elf* elf = elf_begin(fd1, ELF_C_RDWR, NULL);

    if (elf == NULL)
    {
      cerr << elf_errmsg(-1) << endl;
      return 1;
    }

    assert(elf_kind(elf) != ELF_K_AR);
    Elf_Scn *scn;
    Elf_Data *data;
    size_t shstrndx;
    GElf_Shdr shdr;

    assert(elf_getshdrstrndx(elf, &shstrndx ) == 0);
    
    scn = NULL;
    
    while (( scn = elf_nextscn (elf, scn )) != NULL)
    {
      if ( gelf_getshdr ( scn , & shdr ) != & shdr )
      {
        cerr << "getshdr () failed : " << elf_errmsg(-1) << endl;
        return 1;
      }
      
      char *name;
      
      name = elf_strptr (elf, shstrndx, shdr.sh_name);
      
      assert(name);

//       if (string(name) == ".rodata")
//       {
//         data = elf_getdata(scn, NULL);
//         
// //         cout << data->d_size << endl; 
// //         data = elf_newdata(scn);
//         data->d_align = 4;
//         data->d_off = 0;
//         data->d_buf = new char[1024];
//         data->d_type = ELF_T_WORD;
//         data->d_size = 1024;
//         data->d_version = EV_CURRENT;
//       }
//       
//       if (0)
      if (string(name) == ".text")
      {
        stringstream tmpfname;
        tmpfname << "amd_elf_file_" << getpid() << ".tmp";
        
        data = NULL;
        data = elf_getdata(scn, data);
        
        to_file((char*)data->d_buf, data->d_size, tmpfname.str());
        
        int fd2 = open(tmpfname.str().c_str(), O_RDWR);
        
        Elf* elf2 = elf_begin(fd2, ELF_C_RDWR, NULL);
        
//         Elf* elf2 = elf_memory((char*)data->d_buf, data->d_size);
        
        if (elf_kind(elf2) != ELF_K_ELF)
        {
          cerr << "Not a valid amdgpu elf file" << endl;
          return 1;
        }

        {
          Elf_Scn *scn;
          Elf_Data *data;
          size_t shstrndx;
          GElf_Shdr shdr;

          assert(elf_getshdrstrndx(elf2, &shstrndx ) == 0);
          
          scn = NULL;
          int tnum = 0;
          
          while (( scn = elf_nextscn (elf2, scn )) != NULL)
          {
            if ( gelf_getshdr ( scn , & shdr ) != & shdr )
            {
              cerr << "getshdr () failed : " << elf_errmsg(-1) << endl;
              return 1;
            }
            
            char *name;
            
            name = elf_strptr (elf2, shstrndx, shdr.sh_name);
            
            assert(name);
            
            if (string(name) == ".text")
            {
              tnum++;
            }
            
            if (string(name) == ".text" and tnum == 2)
            {
              data = NULL;
              data = elf_getdata(scn, data);
              
              data->d_buf = &binary[0];
              data->d_size =  sizeof(uint32_t)*binary.size();
              
/*              uint32_t* payload_data = (uint32_t*)data->d_buf;
              
              for (int i = 0; i < data->d_size/sizeof(uint32_t); i++)
              {
                code.push_back(payload_data[i]);
              }*/
            }
          }
        }
        
        elf_update(elf2, ELF_C_WRITE);
        data->d_buf = elf_rawfile(elf2, &data->d_size);
        
//         elf_end(elf2);
//         close(fd2);
      }
    }
    
    elf_update(elf, ELF_C_WRITE);
    elf_end(elf);

    close(fd1);
  }
	else
	{
		fwrite(&binary[0], sizeof(uint32_t), binary.size(), f2);
	}	
	
	if (f2)
  {
    fclose(f2);
  }
  
	return 0;
}
