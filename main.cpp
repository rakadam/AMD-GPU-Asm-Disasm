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

#include <string>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <assert.h>
#include <fcntl.h>
#include <gelf.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "gpu_asm.hpp"
#include "r800_def.hpp"

using namespace std;


int main(int argc, char* argv[])
{
  assert(elf_version(EV_CURRENT) != EV_NONE);
  
  string text = r800_def::str();

  gpu_asm::asm_definition asmdef(text);

  gpu_assembler assembler(asmdef);

  vector<uint32_t> code;

  int f = open(argv[1], O_RDONLY);

  if (f < 1)
  {
    cerr << "cannot open file: " << argv[1] << endl;
    return 1;
  }

  Elf* elf = elf_begin(f, ELF_C_READ, NULL);

  if (elf == NULL)
  {
    cerr << elf_errmsg(-1) << endl;
    return 1;
  }

  assert(elf_kind(elf) != ELF_K_AR);

  if (elf_kind(elf) == ELF_K_NONE)
  {
    cerr << "reading raw binary file" << endl;
    
    elf_end(elf);
    close(f);
    uint32_t val;
    
    FILE *f4 = fopen(argv[1], "r");
      
    while (fread(&val, 1, sizeof(uint32_t), f4) > 0)
    {
      code.push_back(val);
    }
    
    fclose(f4);
  }
  else
  {
    cerr << "reading elf file" << endl;
    
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
      
      if (string(name) == ".text")
      {
        data = NULL;
        data = elf_getdata(scn, data);
        Elf* elf2 = elf_memory((char*)data->d_buf, data->d_size);
        
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
              
              uint32_t* payload_data = (uint32_t*)data->d_buf;
              
              for (int i = 0; i < data->d_size/sizeof(uint32_t); i++)
              {
                code.push_back(payload_data[i]);
              }
            }
          }
        }
        
        elf_end(elf2);
      }
    }
    
    elf_end(elf);
    close(f);
  }


  if (code.size() == 0)
  {
    cerr << "Invalid input file: " << argv[1] << endl;
    return 1;
  }

  gpu_disassembler dis(asmdef);

  cout << dis.disassemble(code) << endl << "end;" << endl;
}
