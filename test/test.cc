#include "test.h"

#include <ccfrag/gzip.h>

int main()
{
	std::vector<char> outp;
	std::vector<char> in;
	//ABCDABCABC\n
	std::vector<uint8_t> test_zip = {
0x1f, 0x8b, 0x08, 0x00, 0x1d, 0xda, 0x40, 0x5e, 0x00, 0x03, 0x73, 0x74, 0x72, 0x04, 0x42, 0x2e,
0x00, 0xf5, 0xc8, 0xcc, 0x3a, 0x07, 0x00, 0x00, 0x00
	};
	for(auto it = test_zip.begin(), end = test_zip.end(); it != end; ++it){
		in.push_back(static_cast<char>(*it));
	}
	ccfrag::gzip::decode(outp, in);
	if(!test_framework::execute()) return -1;
	return 0;
}
