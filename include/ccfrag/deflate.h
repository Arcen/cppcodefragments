#pragma once
#include <string>
#include <map>
#include <vector>
#include <deque>
#include <algorithm>

namespace ccfrag{
	// https://tools.ietf.org/html/rfc1951
	// Deutsch, L.P.,"DEFLATE Compressed Data Format Specification", available in ftp://ftp.uu.net/pub/archiving/zip/doc/
	class deflate{
		typedef char symbol_type;
		typedef uint64_t code_type;
		typedef uint16_t literal_type;
		typedef std::pair<code_type, size_t> code_bit_type;
	public:
		class code_info
		{
		public:
			size_t length;
			code_type code;
			code_info()
			: length(0)
			, code(0)
			{
			}
			bool operator<(const code_info& rhs) const
			{
				auto max_length = std::max(length, rhs.length);
				return (code << (max_length - length)) < (rhs.code << (max_length - rhs.length));
			}
			bool operator==(const code_info& rhs) const
			{
				return length == rhs.length && code == rhs.code;
			}
		};
		class huffman_codes
		{
		public:
			std::vector<code_info> codes;
			std::map<code_info, literal_type> table;
			size_t min_length;
			size_t max_length;
			huffman_codes(size_t count)
				: codes(count)
				, table()
				, min_length(0)
				, max_length(0)
			{
			}
			void clear()
			{
				size_t s = codes.size();
				codes.clear();
				codes.resize(s);
				table.clear();
				min_length = max_length = 0;
			}
			bool setup_table()
			{
				if(codes.empty()){
					return false;
				}
				const size_t max_code = codes.size();
				min_length = codes[0].length;
				max_length = min_length;
				for(literal_type i = 0; i < max_code; ++i){
					const auto& code = codes[i];
					const auto& length = code.length;
					if(length){
						table[code] = i;
					}
					min_length = (!min_length) ? length : std::min(min_length, length);
					max_length = std::max(max_length, length);
				}
				if(max_length == 0 && max_length == 0){
					return true;
				}
				if(!min_length || !max_length || 15 < max_length){
					return false;
				}
				return true;
			}
			bool setup_tree()
			{
				const size_t max_code = codes.size();
				// step1
				std::map<size_t, size_t> bl_count;
				for(size_t i = 0; i < max_code; ++i){
					bl_count[codes[i].length]++;
				}
				if(bl_count.empty()){
					return setup_table();//no entry is ok
				}
				size_t max_bits = bl_count.rbegin()->first;
				if(max_bits == 0){
					return false;
				}
				// step2
				std::vector<code_type> next_code(max_bits + 1, 0);
				code_type code = 0;
				bl_count[0] = 0;
				for(int bits = 1; bits <= max_bits; ++bits){
					code = (code + bl_count[bits - 1]) << 1;
					next_code[bits] = code;
				}
				// step3
				for(int n = 0; n < max_code; ++n){
					auto& code = codes[n];
					auto length = code.length;
					if(length){
						code.code = next_code[length]++;
						if(code.code >> code.length){
							return false;
						}
					}
				}
				return setup_table();
			}
			bool setup_fixed_literal_length_table()
			{
				const size_t max_code = 288;
				codes.resize(max_code);
				for(literal_type i = 0; i < 144; ++i){
					codes[i].code = 0x30 + i;				//00110000..10111111
					codes[i].length = 8;
				}
				for(literal_type i = 144; i < 256; ++i){
					codes[i].code = 0x190 + i - 144;		//110010000..111111111
					codes[i].length = 9;
				}
				for(literal_type i = 256; i < 280; ++i){
					codes[i].code = i - 256;				//0000000..0010111
					codes[i].length = 7;
				}
				for(literal_type i = 280; i < 288; ++i){
					codes[i].code = 0xC0 + i - 280;			//11000000..11000111
					codes[i].length = 8;
				}
				return true;
			}
			bool setup_fixed_distance_table()
			{
				const size_t max_code = 32;
				codes.resize(max_code);
				for(literal_type i = 0; i < 32; ++i){
					codes[i].length = 5;
				}
				return setup_tree();
			}
		};
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
			void skip_to_byte_align()
			{
				if(used_bits){
					++begin;
					used_bits = 0;
				}
			}
			// not for huffman codes
			template<typename T>
			bool read(T& data, size_t read_bits)
			{
				if(sizeof(T) * 8 < read_bits || size() < read_bits){
					return false;
				}
				data = 0;
				size_t shift = 0;
				static const uint8_t masks[9] = {0, 0x1, 0x3, 0x7, 0xF, 0x1F, 0x3F, 0x7F, 0xFF};
				while(read_bits){
					auto bits = std::min((8 - used_bits), read_bits);
					data |= static_cast<T>(((static_cast<uint8_t>(*begin) >> used_bits) & masks[bits])) << shift;
					shift += bits;
					read_bits -= bits;
					advance(bits);
				}
				return true;
			}
			template<typename T>
			bool read_literal(T& value, const huffman_codes& hc)
			{
				size_t min_length = hc.min_length;
				size_t max_length = hc.max_length;
				auto& table = hc.table;
				code_info code;
				code.length = min_length;
				if(!read(code.code, min_length)){
					return false;
				}
				auto it = table.find(code);
				while(it == table.end()){
					if(code.length > max_length){
						return false;
					}
					uint8_t c;
					if(!read(c, 1)){
						return false;
					}
					code.code <<= 1;
					code.code |= c;
					++code.length;
					it = table.find(code);
				}
				value = it->second;
				return true;
			}
			template<typename T>
			bool read_extra_bits(T& value, size_t bits)
			{
				value = 0;
				for(size_t i = 0; i < bits; ++i){
					uint8_t c;
					if(!in.read(c, 1)){
						return false;
					}
					value <<= 1;
					value |= c & 1;
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
			bool write(const char * data, size_t length)
			{
				if(used_bits){
					return false;
				}
				output.insert(output.end(), data, data + length);
				return true;
			}
			bool write_reference(size_t before, size_t length)
			{
				if(used_bits){
					return false;
				}
				if(output.size() < before){
					return false;
				}
				size_t offset = output.size() - before;
				while(length){
					size_t block = std::min(output.size() - offset, length);
					std::vector<char> tmp(output.begin() + offset, output.begin() + offset + block);
					output.insert(output.end(), tmp.begin(), tmp.end());
					offset += block;
					length -= block;
				}
				return true;
			}
		};
		static bool encode(std::vector<char>& output, const std::vector<char>& input)
		{
			return false;
		}
		enum{
			BTYPE_NO_COMPRESSION = 0,
			BTYPE_FIXED_HUFFMAN_CODES = 1,
			BTYPE_DYNAMIC_HUFFMAN_CODES = 2,
			BTYPE_RESERVED = 3,
		};
		static bool decode(std::vector<char>& output, const std::vector<char>& input)
		{
			input_type in(input.data(), input.data() + input.size());
			output_type out;
			const size_t max_code = 288;
			huffman_codes fixed_literal_length_hc(max_code);
			huffman_codes dynamic_literal_length_hc(max_code);
			huffman_codes fixed_distance_hc(32);
			huffman_codes dynamic_distance_hc(32);
			if(!fixed_literal_length_hc.setup_fixed_literal_length_table()){
				return false;
			}
			if(!fixed_distance_hc.setup_fixed_distance_table()){
				return false;
			}
			uint8_t BFINAL = 0;
			do {
				int BTYPE;
				if(!in.read(BFINAL, 1) || !in.read(BTYPE, 2)){
					return false;
				}
				if(BTYPE == BTYPE_NO_COMPRESSION){
					//skip any remaining bits in current partially processed byte
					in.skip_to_byte_align();
					//read LEN and NLEN
					uint16_t LEN, NLEN;
					if(!in.read(LEN, 16) || !in.read(NLEN, 16)){
						return false;
					}
					if(LEN != ((~NLEN) & 0xFFFF)){
						return false;
					}
					//copy LEN bytes of data to output
					if(!out.write(in.begin, LEN)){
						return false;
					}
					in.advance(LEN);
				}else{
					huffman_codes & literal_length_hc = (BTYPE == BTYPE_DYNAMIC_HUFFMAN_CODES ? dynamic_literal_length_hc : fixed_literal_length_hc);
					huffman_codes & distance_hc = (BTYPE == BTYPE_DYNAMIC_HUFFMAN_CODES ? dynamic_distance_hc : fixed_distance_hc);
					if(BTYPE == BTYPE_DYNAMIC_HUFFMAN_CODES){
						//read representation of code trees
						//literal/length codes
						//distance codes
						uint16_t HLIT;
						uint16_t HDIST;
						uint16_t HCLEN;
						if(!in.read(HLIT, 5) || !in.read(HDIST, 5) || !in.read(HCLEN, 4)){
							return false;
						}
						HLIT += 257;
						HDIST += 1;
						HCLEN += 4;
						if(286 < HLIT || 32 < HDIST || 19 < HCLEN){
							return false;
						}
						huffman_codes hc_len(19);
						static const size_t hc_index[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
						for(uint16_t i = 0; i < HCLEN; ++i){
							auto& code = hc_len.codes[hc_index[i]];
							auto& length = code.length;
							if(!in.read(length, 3)){
								return false;
							}
						}
						if(!hc_len.setup_tree()){
							return false;
						}
						size_t literal_distance_lengths[286+32] = {};
						for(size_t i = 0; i < HLIT + HDIST; ){
							auto& length = literal_distance_lengths[i];
							if(!in.read_literal(length, hc_len)){
								return false;
							}
							if(length < 16){
								++i;
							}else if(length == 16){
								if(i == 0){
									return false;
								}
								size_t src = literal_distance_lengths[i-1];
								size_t count;
								if(!in.read(count, 2)){
									return false;
								}
								for(size_t end = i + count; i < end && i < HLIT + HDIST; ++i){
									literal_distance_lengths[i] = src;
								}
							}else if(length == 17 || length == 18){
								size_t count;
								if(!in.read(count, length == 17 ? 3 : 7)){
									return false;
								}
								for(size_t end = i + count; i < end && i < HLIT + HDIST; ++i){
									literal_distance_lengths[i] = 0;
								}
							}else{
								return false;
							}
						}
						dynamic_literal_length_hc.clear();
						dynamic_distance_hc.clear();
						for(uint16_t i = 0; i < HLIT; ++i){
							dynamic_literal_length_hc.codes[i].length = literal_distance_lengths[HLIT + i];
						}
						if(!dynamic_literal_length_hc.setup_tree()){
							return false;
						}
						for(uint16_t i = 0; i < HDIST; ++i){
							dynamic_distance_hc.codes[i].length = literal_distance_lengths[HLIT+i];
						}
						if(!dynamic_distance_hc.setup_tree()){
							return false;
						}
					}else if(BTYPE != BTYPE_FIXED_HUFFMAN_CODES){
						return false;
					}
					while(true){
						static const int end_of_block = 256;
						//decode literal/length value from input stream
						literal_type value;
						if(!in.read_literal(value, *hc)){
							return false;
						}
						if(value < 256){
							//copy value (literal byte) to output stream
							char c = value;
							if(!out.write(&c, 1)){
								return false;
							}
						}else if(value == end_of_block){
							break;
						}else if(257 <= value && value <= 285){//257..285
							static const size_t length_extra_bits_table[29] = {
								0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
								1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
								4, 4, 4, 4, 5, 5, 5, 5, 0
							};
							static const uint16_t length_base_table[29] = {
								3, 4, 5, 6, 7, 8, 9, 10, 11, 13,
								15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
								67, 83, 99, 115, 131, 163, 195, 227, 258
							};
							uint16_t length = 0;
							if(in.read_extra_bits(length, length_extra_bits_table[value - 257])){
								return false;
							}
							length += length_base_table[value - 257];
							//decode distance from input stream
							literal_type distance_value;
							if(!in.read(distance_value, 5)){
								return false;
							}
							if(30 <= distance_value){
								return false;
							}
							static const uint16_t distance_base_table[30] = {
								1, 2, 3, 4, 5, 7, 9, 13, 17, 25,
								33, 49, 65, 97, 129, 193, 257, 385, 513, 768,
								1024, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
							};
							uint16_t distance = 0;
							if(in.read_extra_bits(distance, distance < 4 ? 0 : (distance / 2 - 1))){
								return false;
							}
							distance += distance_base_table[distance_value];
							//move backwards distance bytes in the output stream, 
							// and copy length bytes from this position to the output stream.
							if(!out.write_reference(distance, length)){
								return false;
							}
						}
					}
				}
			} while(!BFINAL);
			output.assign(out.output.begin(), out.output.end());
			return true;
		}
	};
}
