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

#include <fstream>
#include <iostream>
#include <sstream>
#include <cmath>
#include <vector>
#include <cstdlib>

using namespace std;




struct usage_t{
public:
	static std::string str()
        {
            const char data[]={
                'U','s','a','g','e',':',' ',0x0
            };
            
            return std::string(data,sizeof(data)-1);
	}
};



int main(int argc, char *argv[])
{
  

    stringstream ss; ss.clear();
    for (int i=1; i<argc; i++) ss << argv[i] << " ";

    string infilename,structname,outfilename;
    ss >> infilename >> structname >> outfilename;
    cout << infilename << endl;
    if (!ss.good()) {
		cerr << usage_t::str() << argv[0] << " infilename structname outfilename"<<endl;
		return 1;
    }
    {
        ifstream f(infilename.c_str(),ios::binary);
        if (f.fail()) {
            cerr<< "file open error " << infilename << endl;
               return 1;
        }
    }

    FILE* infile=fopen(infilename.c_str(),"rb");
    if (!infile) {
        return 1;
    }
    

    ofstream outfile(outfilename.c_str(),ios::trunc);
    unsigned char buff[1];

		vector<unsigned char> code;
		
		srand(time(0));
		
    outfile <<
        "#include <string>"             << endl <<
        "struct " << structname << "{"   << endl <<
        "public:"                       << endl <<
	"   static std::string str()"      << endl <<
        "    {"                             << endl <<
        "    const static unsigned char data[]={"       << endl;
    int count=0;
    while(1){
        int e = fread(buff, 1, 1, infile);
	if (e < 1) break;

        int it=(int)buff[0];

//         if (it>127){
//             cout << it << count << endl;
//             return 1;
//         }
//         if (it<0){
//             cout << it << count << endl;
//             return 1;
//         }

	code.push_back(rand()%256);
	
        outfile << "0x" << hex << (it^code.back()) << ",";
        count ++;

        if (count % 10==0){
            outfile << endl;
        }
        
        outfile << "0x" << hex << (int)code.back() << ",";
        count ++;

        if (count % 10==0){
            outfile << endl;
        }
     }

    
    outfile <<    "        "                      << endl;
    outfile <<    "       };"                        << endl;
    outfile <<    "       "                          << endl;
    outfile <<    "       "                          << endl;
    outfile <<    "std::string str; "<< endl;
		outfile <<    "for (unsigned int i = 0; i < sizeof(data)-1; i+=2) "<< endl;
		outfile <<    "	str += (char)(data[i]^data[i+1]); "<< endl;
    outfile <<    "       return str;" << endl;
    outfile <<    "   }"                             << endl;
    outfile <<        "};" << endl;


    cout << count;
    fclose(infile);
    cout << "done." << endl;
    return 0;
}
