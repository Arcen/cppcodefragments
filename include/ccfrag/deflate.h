#pragma once
#include <string>
#include <map>
#include <vector>
#include <deque>
#include <set>
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
		class code_info{
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
				if(length != rhs.length) return length < rhs.length;
				return code < rhs.code;
			}
			bool operator==(const code_info& rhs) const
			{
				return length == rhs.length && code == rhs.code;
			}
		};
		enum{
			BTYPE_NO_COMPRESSION = 0,
			BTYPE_FIXED_HUFFMAN_CODES = 1,
			BTYPE_DYNAMIC_HUFFMAN_CODES = 2,
			BTYPE_RESERVED = 3,
			MAX_BITS = 15,
			MIN_LENGTH = 3,
			MAX_LENGTH = 258,
			MIN_DISTANCE = 1,
			MAX_DISTANCE = 32768,
			END_OF_BLOCK = 256,
			MAX_BLOCK_SIZE = 65535,
			MAX_DISTANCE_CODE = 32,
			MAX_LITERAL_CODE = 288,
		};
		class huffman_codes{
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
					fprintf(stderr, "empty table\n");
					return false;
				}
				const size_t max_code = codes.size();
				min_length = codes[0].length;
				max_length = min_length;
				for(literal_type i = 0; i < max_code; ++i){
					const auto& code = codes[i];
					const auto& length = code.length;
					min_length = (!min_length || !length) ? std::max(min_length, length) : std::min(min_length, length);
					max_length = std::max(max_length, length);
					if(length){
						table[code] = i;
					}
				}
				if(max_length == 0 && max_length == 0){
					return true;
				}
				if(!min_length || !max_length || MAX_BITS < max_length){
					fprintf(stderr, "invalid length %zd, %zd\n", min_length, max_length);
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
					if(codes[i].length){
						if(MAX_BITS < codes[i].length){
							fprintf(stderr, "over flow max bits %zd\n", codes[i].length);
							return false;
						}
						bl_count[codes[i].length]++;
					}
				}
				if(bl_count.empty()){
					return setup_table();//no entry is ok
				}
				// step2
				std::vector<code_type> next_code(MAX_BITS + 1, 0);
				code_type code = 0;
				bl_count[0] = 0;
				for(int bits = 1; bits <= MAX_BITS; ++bits){
					code = (code + bl_count[bits - 1]) << 1;
					next_code[bits] = code;
				}
				// step3
				for(int n = 0; n < max_code; ++n){
					auto& code = codes[n];
					auto length = code.length;
					if(length){
						code.code = next_code[length];
						next_code[length]++;
						if(code.code >> code.length){
							for(auto it = bl_count.begin(), end = bl_count.end(); it != end; ++it){
								fprintf(stderr, "bl_count[%zd]=%zd, next_code[%zd]=0x%zx\n", it->first, it->second, it->first, next_code[it->first]);
							}
							fprintf(stderr, "overflow tree, code=0x%x len=%d\n", code.code, code.length);
							return false;
						}
					}
				}
				return setup_table();
			}
			bool setup_fixed_literal_length_table()
			{
				const size_t max_code = MAX_LITERAL_CODE;
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
				return setup_table();
			}
			bool setup_fixed_distance_table()
			{
				const size_t max_code = MAX_DISTANCE_CODE;
				codes.resize(max_code);
				for(literal_type i = 0; i < max_code; ++i){
					codes[i].length = 5;
				}
				return setup_tree();
			}
		};
		class encoder{
		public:
			class input_type{
			public:
				const char * begin;
				const char * end;
				input_type(const input_type& rhs)
					: begin(rhs.begin)
					, end(rhs.end)
				{
				}
				input_type(const char* begin, const char* end)
					: begin(begin)
					, end(end)
				{
				}
				bool empty() const
				{
					return end <= begin;
				}
				size_t size() const
				{
					return empty() ? 0 : (end - begin);
				}
				void advance(size_t count)
				{
					begin += count;
				}
				size_t match(const input_type& target, size_t max_length) const
				{
					size_t length = 0;
					max_length = std::min(max_length, std::min(size(), target.size()));
					while(length < max_length){
						if(begin[length] != target.begin[length]){
							break;
						}
						++length;
					}
					return length;
				}
			};
			class output_type{
			public:
				std::deque<char> output;
				uint8_t byte;
				size_t used_bits;
				output_type()
					: byte(0)
					, used_bits(0)
				{
				}
				template<typename T>
				bool write(T write_data, size_t write_bits)
				{
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
				bool write_code(const code_info& code)
				{
					for(size_t i = 0; i < code.length; ++i){
						if(!write(((code.code >> (code.length - 1 - i)) & 1), 1)){
							return false;
						}
					}
					return true;
				}
				bool write_length(const huffman_codes& hc, size_t length)
				{
					if(length < MIN_LENGTH || MAX_LENGTH < length){
						return false;
					}
					static const size_t value_table[6] = {257, 265, 269, 273, 277, 281};
					static const size_t offsets[7] = {3, 11, 19, 35, 67, 131, 258};
					literal_type value = 285;
					size_t extra_bits = 0;
					size_t extra_data = 0;
					for(size_t i = 0; i < 6; ++i){
						if(length < offsets[i+1]){
							value = value_table[i] + (length - offsets[i]) / (1 << i);
							extra_bits = i;
							extra_data = (length - offsets[i]) % (1 << i);
							break;
						}
					}
					const auto& code = hc.codes[value];
					if(!write_code(code)){
						return false;
					}
					if(extra_bits && !write(extra_data, extra_bits)){
						return false;
					}
					return true;
				}
				bool write_distance(const huffman_codes& hc, size_t distance)
				{
					if(distance < MIN_DISTANCE || MAX_DISTANCE < distance){
						return false;
					}
					static const size_t value_table[14] = {0, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28};
					static const size_t offsets[15] = {1, 5, 9, 17, 33, 65, 129, 257, 513, 1025, 2049, 4097, 8193, 16385, 32769};
					literal_type value = 30;
					size_t extra_bits = 0;
					size_t extra_data = 0;
					for(size_t i = 0; i < 14; ++i){
						if(distance < offsets[i + 1]){
							value = value_table[i] + (distance - offsets[i]) / (1 << i);
							extra_bits = i;
							extra_data = (distance - offsets[i]) % (1 << i);
							break;
						}
					}
					if(value == 30){
						return false;
					}
					const auto& code = hc.codes[value];
					if(!write_code(code)){
						return false;
					}
					if(extra_bits && !write(extra_data, extra_bits)){
						return false;
					}
					return true;
				}
			};
			static bool encode(std::vector<char>& output, const std::vector<char>& input)
			{
				input_type in(input.data(), input.data() + input.size());
				output_type out;
				huffman_codes fixed_literal_length_hc(MAX_LITERAL_CODE);
				huffman_codes fixed_distance_hc(MAX_DISTANCE_CODE);
				if(!fixed_literal_length_hc.setup_fixed_literal_length_table()){
					return false;
				}
				if(!fixed_distance_hc.setup_fixed_distance_table()){
					return false;
				}
				std::map<uint32_t, std::set<size_t> > last_pattern;
				while(!in.empty()){
					input_type block(in.begin, in.begin + std::min<size_t>(in.size(), MAX_BLOCK_SIZE));
					in.advance(block.size());
					uint8_t BFINAL = (in.empty() ? 1 : 0);
					uint8_t BTYPE = BTYPE_FIXED_HUFFMAN_CODES;
					out.write(BFINAL, 1);
					out.write(BTYPE, 2);
					while(!block.empty()){
						size_t offset_from_start = block.begin - input.data();
						size_t reference_start = offset_from_start < MAX_DISTANCE ? 0 : offset_from_start - MAX_DISTANCE;
						bool wrote = false;
						if(3 <= block.size() || 3 <= in.end - block.begin){
							uint32_t start_code = 
								(static_cast<uint8_t>(block.begin[0]) << 16) |
								(static_cast<uint8_t>(block.begin[1]) << 8) |
								(static_cast<uint8_t>(block.begin[2]));
							auto it = last_pattern.find(start_code);
							if(3 <= block.size() && it != last_pattern.end()){
								auto& patterns = it->second;
								size_t best_distance = 0;
								size_t best_length = 0;
								for(auto pit = patterns.begin(); pit != patterns.end();){
									size_t position = *pit;
									if(reference_start <= position){
										uint32_t distance = offset_from_start - position;
										input_type src(input.data() + position, in.end);
										size_t length = src.match(block, MAX_LENGTH);
										if(best_length < length){
											best_length = length;
											best_distance = distance;
										}
										++pit;
									}else{
										pit = patterns.erase(pit);
									}
								}
								if(3 <= best_length){
									block.advance(best_length);
									if(!out.write_length(fixed_literal_length_hc, best_length)){
										return false;
									}
									if(!out.write_distance(fixed_distance_hc, best_distance)){
										return false;
									}
									wrote = true;
								}
								patterns.insert(offset_from_start);
							}else{
								last_pattern[start_code].insert(offset_from_start);
							}
						}
						if(!wrote){
							uint8_t data = static_cast<uint8_t>(*block.begin);
							block.advance(1);
							auto& code = fixed_literal_length_hc.codes[data];
							if(!out.write_code(code)){
								return false;
							}
						}
					}
					if(!out.write_code(fixed_literal_length_hc.codes[END_OF_BLOCK])){
						return false;
					}
				}
				out.flush();
				output.assign(out.output.begin(), out.output.end());
				return true;
			}
		};
		class decoder{
		public:
			class input_type{
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
					for(size_t i = 0; i < min_length; ++i){
						uint8_t c;
						if(!read(c, 1)){
							fprintf(stderr, "not enough length %zd\n", i);
							return false;
						}
						code.code <<= 1;
						code.code |= c;
					}
					auto it = table.find(code);
					while(it == table.end()){
						if(code.length >= max_length){
							fprintf(stderr, "overflow length=%zd code=%zx, size=%zd\n", code.length, code.code, table.size());
							return false;
						}
						uint8_t c = 0;
						if(!read(c, 1)){
							fprintf(stderr, "not enough length %zd\n", code.length);
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
			};
			class output_type{
			public:
				std::deque<char> output;
				output_type()
				{
				}
				bool write(const char * data, size_t length)
				{
					output.insert(output.end(), data, data + length);
					return true;
				}
				bool write_reference(size_t before, size_t length)
				{
					if(output.size() < before){
						return false;
					}
					size_t offset = output.size() - before;
					while(length){
						output.insert(output.end(), output[offset]);
						--length;
						++offset;
					}
					return true;
				}
			};
			static bool decode(std::vector<char>& output, const std::vector<char>& input)
			{
				input_type in(input.data(), input.data() + input.size());
				output_type out;
				huffman_codes fixed_literal_length_hc(MAX_LITERAL_CODE);
				huffman_codes dynamic_literal_length_hc(MAX_LITERAL_CODE);
				huffman_codes fixed_distance_hc(MAX_DISTANCE_CODE);
				huffman_codes dynamic_distance_hc(MAX_DISTANCE_CODE);
				if(!fixed_literal_length_hc.setup_fixed_literal_length_table()){
					return false;
				}
				if(!fixed_distance_hc.setup_fixed_distance_table()){
					return false;
				}
				uint8_t BFINAL = 0;
				do{
					int BTYPE;
					if(!in.read(BFINAL, 1) || !in.read(BTYPE, 2)){
						fprintf(stderr, "read failed BFINAL, BTYPE\n");
						return false;
					}
					if(BTYPE == BTYPE_NO_COMPRESSION){
						//skip any remaining bits in current partially processed byte
						in.skip_to_byte_align();
						//read LEN and NLEN
						uint16_t LEN, NLEN;
						if(!in.read(LEN, 16) || !in.read(NLEN, 16)){
							fprintf(stderr, "read failed LEN, NLEN\n");
							return false;
						}
						if(LEN != ((~NLEN) & 0xFFFF)){
							fprintf(stderr, "LEN, NLEN error %04x %04x\n", LEN, NLEN);
							return false;
						}
						//copy LEN bytes of data to output
						if(!out.write(in.begin, LEN)){
							return false;
						}
						in.advance(LEN);
					} else{
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
								fprintf(stderr, "read HLIT, HDIST, HCLEN error %04x %04x\n", HLIT, HDIST, HCLEN);
								return false;
							}
							HLIT += 257;
							HDIST += 1;
							HCLEN += 4;
							if(MAX_LITERAL_CODE < HLIT || MAX_DISTANCE_CODE < HDIST || 19 < HCLEN){
								fprintf(stderr, "invalid HLIT, HDIST, HCLEN error %04x %04x\n", HLIT, HDIST, HCLEN);
								return false;
							}
							huffman_codes hc_len(19);
							static const size_t hc_index[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
							for(uint16_t i = 0; i < HCLEN; ++i){
								auto& code = hc_len.codes[hc_index[i]];
								auto& length = code.length;
								if(!in.read(length, 3)){
									fprintf(stderr, "read error HC\n");
									return false;
								}
							}
							if(!hc_len.setup_tree()){
								fprintf(stderr, "failed to hc_len setup\n");
								return false;
							}
							size_t literal_distance_lengths[MAX_LITERAL_CODE + MAX_DISTANCE_CODE] = {};
							for(size_t i = 0; i < HLIT + HDIST; ){
								auto& length = literal_distance_lengths[i];
								if(!in.read_literal(length, hc_len)){
									fprintf(stderr, "failed to read literal for HLIT, HDIST\n");
									return false;
								}
								if(length < 16){
									++i;
								} else if(length == 16){
									if(i == 0){
										fprintf(stderr, "invalid reference for HLIT/HDIST\n");
										return false;
									}
									size_t src = literal_distance_lengths[i - 1];
									size_t count;
									if(!in.read(count, 2)){
										return false;
									}
									count += 3;
									for(size_t end = i + count; i < end && i < HLIT + HDIST; ++i){
										literal_distance_lengths[i] = src;
									}
								} else if(length == 17 || length == 18){
									size_t count;
									if(!in.read(count, length == 17 ? 3 : 7)){
										return false;
									}
									count += (length == 17 ? 3 : 11);
									for(size_t end = i + count; i < end && i < HLIT + HDIST; ++i){
										literal_distance_lengths[i] = 0;
									}
								} else{
									fprintf(stderr, "invalid hc_len %zd\n", length);
									return false;
								}
							}
							dynamic_literal_length_hc.clear();
							dynamic_distance_hc.clear();
							for(uint16_t i = 0; i < HLIT; ++i){
								dynamic_literal_length_hc.codes[i].length = literal_distance_lengths[i];
							}
							for(uint16_t i = HLIT; i < MAX_LITERAL_CODE; ++i){
								dynamic_literal_length_hc.codes[i].length = 0;
							}
							if(!dynamic_literal_length_hc.setup_tree()){
								fprintf(stderr, "failed to dynamic_literal_length_hc setup\n");
								return false;
							}
							for(uint16_t i = 0; i < HDIST; ++i){
								dynamic_distance_hc.codes[i].length = literal_distance_lengths[HLIT + i];
							}
							for(uint16_t i = HDIST; i < MAX_DISTANCE_CODE; ++i){
								dynamic_distance_hc.codes[i].length = 0;
							}
							if(!dynamic_distance_hc.setup_tree()){
								fprintf(stderr, "failed to dynamic_distance_hc setup\n");
								return false;
							}
						} else if(BTYPE != BTYPE_FIXED_HUFFMAN_CODES){
							fprintf(stderr, "invalid BTYPE %d\n", BTYPE);
							return false;
						}
						while(true){
							//decode literal/length value from input stream
							literal_type value;
							if(!in.read_literal(value, literal_length_hc)){
								fprintf(stderr, "failed to read literal \n");
								return false;
							}
							if(value < END_OF_BLOCK){
								//copy value (literal byte) to output stream
								char c = value;
								if(!out.write(&c, 1)){
									fprintf(stderr, "output error\n");
									return false;
								}
							} else if(value == END_OF_BLOCK){
								break;
							} else if(END_OF_BLOCK < value && value <= 285){//257..285
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
								size_t length_extra_bits = length_extra_bits_table[value - 257];
								if(!in.read(length, length_extra_bits)){
									fprintf(stderr, "failed to read length extra bits %zd\n", length_extra_bits);
									return false;
								}
								length += length_base_table[value - 257];
								//decode distance from input stream
								literal_type distance_value;
								if(!in.read_literal(distance_value, distance_hc)){
									return false;
								}
								if(30 <= distance_value){
									fprintf(stderr, "distance is invalid %d\n", distance_value);
									return false;
								}
								static const uint16_t distance_base_table[30] = {
									1, 2, 3, 4, 5, 7, 9, 13, 17, 25,
									33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
									1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
								};
								uint16_t distance = 0;
								size_t distance_extra_bits = distance_value < 4 ? 0 : (distance_value / 2 - 1);
								if(!in.read(distance, distance_extra_bits)){
									fprintf(stderr, "failed to read distance extra bits %zd\n", distance_extra_bits);
									return false;
								}
								distance += distance_base_table[distance_value];
								//move backwards distance bytes in the output stream, 
								// and copy length bytes from this position to the output stream.
								if(!out.write_reference(distance, length)){
									fprintf(stderr, "reference copy failed currentsize=%zd, distance=%zd lenght=%zd\n", out.output.size(), distance, length);
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
		static bool encode(std::vector<char>& output, const std::vector<char>& input)
		{
			return encoder::encode(output, input);
		}
		static bool decode(std::vector<char>& output, const std::vector<char>& input)
		{
			return decoder::decode(output, input);
		}
	};
}
