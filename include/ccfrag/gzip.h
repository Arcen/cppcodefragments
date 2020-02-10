#pragma once
#include <string>
#include <map>
#include <vector>
#include <deque>
#include <algorithm>
#include <ccfrag/deflate.h>

namespace ccfrag{
	// https://tools.ietf.org/html/rfc1952
	class gzip{
	public:
		class crc32
		{
			uint32_t crc_table[256];
		public:
			crc32()
			{
				for(uint32_t n = 0; n < 256; ++n){
					uint32_t c = n;
					for(int k = 0; k < 8; ++k){
						if(c & 1){
							c = 0xedb88320L ^ (c >> 1);
						} else{
							c >>= 1;
						}
					}
					crc_table[n] = c;
					fprintf(stderr, "0x%08x, ", c);
					fprintf(stderr, "0x%08x, ", c);
				}
				fprintf(stderr, "\n");
			}
			template<typename I>
			uint32_t execute(I begin, I end, uint32_t c = 0) const
			{
				c ^= 0xFFFFFFFFL;
				while(begin != end){
					uint8_t u = static_cast<uint8_t>(*begin);
					++begin;
					c = crc_table[(c ^ u) & 0xFF] ^ (c >> 8);
				}
				return c ^ 0xFFFFFFFFL;
			}
		};
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
			template<typename T>
			bool read(T& data, size_t count = sizeof(T))
			{
				if(sizeof(data) < count || size() < count){
					return false;
				}
				data = 0;
				size_t shift = 0;
				while(count){
					data |= static_cast<T>(*begin) << shift;
					shift += 8;
					--count;
					advance(1);
				}
				return true;
			}
			bool read(std::string& data)
			{
				char c;
				while(true){
					if(!read(c)){
						return false;
					}
					if(!c) {
						break;
					}
					data.push_back(c);
				}
				return true;
			}
		};
		enum{
			CM_DEFLATE = 8,
			FLG_FTEXT = 1 << 0,
			FLG_FHCRC = 1 << 1,
			FLG_FEXTRA = 1 << 2,
			FLG_FNAME = 1 << 3,
			FLG_FCOMMENT = 1 << 4,
			FLG_RESERVED = 0xE0,
			XFL_SLOW = 2,
			XFL_FAST = 4,
		};
		static bool encode(std::vector<char>& output, const std::vector<char>& input)
		{
			return false;
		}
		static bool decode(std::vector<char>& output, const std::vector<char>& input)
		{
			input_type in(input.data(), input.data() + input.size());
			uint8_t ID1;
			if(!in.read(ID1) || ID1 != 0x1F){
				return false;
			}
			uint8_t ID2;
			if(!in.read(ID2) || ID2 != 0x8B){
				return false;
			}
			uint8_t CM;
			if(!in.read(CM) || CM != CM_DEFLATE){
				return false;
			}
			uint8_t FLG;
			if(!in.read(FLG) || FLG & FLG_RESERVED){
				return false;
			}
			uint32_t MTIME; // Modification TIME
			if(!in.read(MTIME)){
				return false;
			}
			uint8_t XFL; // eXtra FLags
			if(!in.read(XFL) || (XFL != XFL_SLOW && XFL != XFL_FAST)){
				return false;
			}
			uint8_t OS;
			if(!in.read(OS)){
				return false;
			}
			if(FLG & FLG_FEXTRA){
				uint16_t XLEN;
				if(!in.read(XLEN)){
					return false;
				}
			}
			if(FLG & FLG_FNAME){
				std::string fname;
				if(!in.read(fname)){
					return false;
				}
			}
			if(FLG & FLG_FCOMMENT){
				std::string fname;
				if(!in.read(fname)){
					return false;
				}
			}
			crc32 crc;
			if(FLG & FLG_FHCRC){
				uint16_t CRC16;
				if(!in.read(CRC16)){
					return false;
				}
				uint32_t c = crc.execute(input.data(), input.data() + input.size() - in.size()) & 0xFFFF;
				if(c != CRC16){
					return false;
				}
			}
			if(in.size() < 8){
				return false;
			}
			size_t header = input.size() - in.size();
			size_t footer = 8;
			if(!ccfrag::deflate::decode(output, {in.begin, in.end - 8})){
				return false;
			}
			in.advance(in.size() - 8);
			uint32_t CRC32;
			if(!in.read(CRC32)){
				return false;
			}
			uint32_t c = crc.execute(output.data(), output.data() + output.size());
			if(c != CRC32){
				return false;
			}
			uint32_t ISIZE;
			if(!in.read(ISIZE)){
				return false;
			}
			if(output.size() & 0xFFFFFFFF != ISIZE){
				return false;
			}
			return true;
		}
	};
}
