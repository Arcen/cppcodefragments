#include <ccfrag/compress.h>

using namespace ccfrag;

int main(int argc, char *argv[])
{
	FILE * fin = stdin;
	FILE * fout = stdout;
	int c;
	std::deque<char> input;
	while((c = fgetc(fin)) != EOF){
		input.push_back(c);
	}
	std::vector<char> input_data(input.begin(), input.end());
	std::vector<char> output;
	bool decode = false;
	for(int i = 1; i < argc; ++i){
		if(std::string("-d") == argv[i]){
			decode = true;
		}
	}
	if(decode){
		if(!compress::decode(output, input_data)){
			return -1;
		}
	}else{
		if(!compress::encode(output, input_data)){
			return -1;
		}
	}
	fwrite(output.data(), 1, output.size(), fout);
	return 0;
}
