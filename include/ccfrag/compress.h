#pragma once
#include <string>
#include <map>
#include <vector>
#include <deque>
#include <algorithm>

namespace ccfrag{
	// https://xlinux.nist.gov/dads/HTML/lempelZivWelch.html
	// https://marknelson.us/posts/2011/11/08/lzw-revisited.html
	// https://vapier.github.io/ncompress/
	class compress{
		typedef char symbol_type;
		typedef uint32_t code_type;
		typedef std::pair<code_type, size_t> code_bit_type;
	public:
		class input_type
		{
		public:
			const char * begin;
			const char * end;
			size_t used_bits;
			input_type(const input_type& rhs)
				: begin(rhs.begin)
				, end(rhs.end)
				, used_bits(rhs.used_bits)
			{
			}
			input_type(const char* begin, const char* end)
				: begin(begin)
				, end(end)
				, used_bits(0)
			{
			}
			bool empty() const
			{
				return end <= begin;
			}
			size_t size() const
			{
				return empty() ? 0 : (end - begin) * 8 - used_bits;
			}
			void advance(size_t count)
			{
				used_bits += count;
				begin += used_bits / 8;
				used_bits %= 8;
			}
			bool read(code_type& data, size_t read_bits)
			{
				if(64 < read_bits || size() < read_bits){
					return false;
				}
				data = 0;
				size_t shift = 0;
				static const uint8_t masks[9] = {0, 0x1, 0x3, 0x7, 0xF, 0x1F, 0x3F, 0x7F, 0xFF};
				while(read_bits){
					auto bits = std::min((8 - used_bits), read_bits);
					data |= ((*begin >> used_bits) & masks[bits]) << shift;
					shift += bits;
					read_bits -= bits;
					advance(bits);
				}
				return true;
			}
		};
		class output_type
		{
		public:
			std::deque<char> output;
			uint8_t byte;
			size_t used_bits;
			output_type()
				: byte(0)
				, used_bits(0)
			{
			}
			bool write(const code_bit_type& data)
			{
				auto write_data = data.first;
				auto write_bits = data.second;
				if(32 < write_bits){
					return false;
				}
				static const uint8_t masks[9] = {0, 0x1, 0x3, 0x7, 0xF, 0x1F, 0x3F, 0x7F, 0xFF};
				while(write_bits){
					auto bits = std::min((8 - used_bits), write_bits);
					byte |= (write_data & masks[bits]) << used_bits;
					used_bits += bits;
					write_bits -= bits;
					write_data >>= bits;
					if(used_bits == 8){
						flush();
					}
				}
				return true;
			}
			void flush()
			{
				if(used_bits){
					output.push_back(byte);
					used_bits = 0;
					byte = 0;
				}
			}
		};
		static bool encode(std::vector<char>& output, const std::vector<char>& input, char max_bits = 16, bool block_mode = true)
		{
			// header
			static const code_bit_type magic_number1 = code_bit_type(0x1f, 8);
			static const code_bit_type magic_number2 = code_bit_type(0x9d, 8);
			output_type out;
			if(max_bits < 9) max_bits = 9;
			if(16 < max_bits) max_bits = 16;
			code_type max_code = (static_cast<code_type>(1) << max_bits) - 1;
			out.write(magic_number1);
			out.write(magic_number2);
			out.write(code_bit_type((block_mode ? 0x80 : 0)| max_bits, 8));
			std::map<std::string, code_type> codes;
			for(code_type i = 0; i < 256; ++i){
				codes[std::string(1, static_cast<symbol_type>(i))] = i;
			}
			code_type next_code = 257; // 256 is reserved for EOF(table setup again)
			size_t current_max_bits = 9;
			std::string current_string;
			for(auto it = input.begin(), end = input.end(); it != end; ++it){
				symbol_type c = *it;
				current_string.push_back(c);
				if((static_cast<code_type>(1) << current_max_bits) < next_code){
					if(current_max_bits < 16){
						++current_max_bits;
					}
				}
				auto cit = codes.find(current_string);
				if(cit == codes.end()){ // not found in dictionary
					if(next_code <= max_code){
						codes[current_string] = next_code++;// add new code.
					} // else for checking output EOF
					current_string.pop_back(); // change to word in dictionary
					cit = codes.find(current_string);
					if(cit == codes.end()){
						return false;
					}
					out.write(code_bit_type(cit->second, current_max_bits));
					current_string = c;
				}
			}
			auto cit = codes.find(current_string);
			if(cit == codes.end()){
				return false;
			}
			out.write(code_bit_type(cit->second, current_max_bits));
			out.flush();
			output.assign(out.output.begin(), out.output.end());
			return true;
		}
		static bool decode(std::vector<char>& output, const std::vector<char>& input)
		{
			input_type in(input.data(), input.data() + input.size());
			static const code_type eof_code = 256;
			static const code_type magic_number1 = 0x1F;
			static const code_type magic_number2 = 0x9D;
			code_type code;
			if(!in.read(code, 8) || code != magic_number1){
				return false;
			}
			if(!in.read(code, 8) || code != magic_number2){
				return false;
			}
			if(!in.read(code, 8)){
				return false;
			}
			const bool block_mode = (code & 0x80 ? true : false);
			const char max_bits = (code & 0x7F);
			if(max_bits < 9 || 16 < max_bits) {
				return false;
			}
			const code_type max_code = (static_cast<code_type>(1) << max_bits) - 1;
			std::map<code_type, std::string> strings;
			for(code_type i = 0; i < 256; ++i){
				strings.insert(std::make_pair(i, std::string(1, i)));
			}
			std::string previous_string;
			size_t current_max_bits = 9;
			code_type next_code = block_mode ? 257 : 256;
			std::deque<char> out;
			size_t read_size = 0;
			while(in.read(code, current_max_bits)){
				read_size += current_max_bits;
				if(block_mode && code == eof_code){
					size_t block_size = current_max_bits * 8;
					read_size %= block_size;
					if(read_size){//align to block
						in.advance(block_size - read_size);
						read_size = 0;
					}
					current_max_bits = 9;
					next_code = 256;
					continue;
				}
				auto sit = strings.find(code);
				if(sit == strings.end() || next_code <= code){
					if(previous_string.empty()){
						fprintf(stderr, "found previous_string empty on unknown code 0x%x , strings size %zd\n", code, strings.size());
						return false;
					}
					strings[code] = previous_string + previous_string[0];
					sit = strings.find(code);
				}
				auto& string = sit->second;
				out.insert(out.end(), string.begin(), string.end());
				if(!previous_string.empty() && next_code <= max_code){
					strings[next_code++] = previous_string + string[0];
					if((static_cast<code_type>(1) << current_max_bits) - 1 < next_code){
						if(current_max_bits < 16){
							++current_max_bits;
						}
					}
				}
				previous_string = string;
			}
			output.assign(out.begin(), out.end());
			return true;
		}
	};
}
