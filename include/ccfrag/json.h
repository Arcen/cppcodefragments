#pragma once
#include <string>
#include <map>
#include <deque>
#include <memory>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <functional>
#include <limits>
#include <string.h>
#include <math.h>
#include <limits.h>

namespace ccfrag {
	// specification
	// https://tools.ietf.org/html/rfc8259
	// https://www.json.org/json-en.html
	namespace json {
		struct unicode_tools {
			// https://ja.wikipedia.org/wiki/UTF-8
			static std::string ucs4_to_utf8(uint32_t ucs4)
			{
				std::string result;
				if(ucs4 <= 0x7F){
					result += static_cast<char>(ucs4);
					return result;
				}
				if(ucs4 <= 0x7FF){
					result += static_cast<char>(((ucs4 >> 6) & 0x1F) | 0xC0);
					result += static_cast<char>(((ucs4 >> 0) & 0x3F) | 0x80);
					return result;
				}
				if(ucs4 <= 0xFFFF){
					result += static_cast<char>(((ucs4 >> 12) & 0xF) | 0xE0);
					result += static_cast<char>(((ucs4 >> 6) & 0x3F) | 0x80);
					result += static_cast<char>(((ucs4 >> 0) & 0x3F) | 0x80);
					return result;
				}
				if(ucs4 <= 0x1FFFFF){
					result += static_cast<char>(((ucs4 >> 18) & 0x7) | 0xF0);
					result += static_cast<char>(((ucs4 >> 12) & 0x3F) | 0x80);
					result += static_cast<char>(((ucs4 >> 6) & 0x3F) | 0x80);
					result += static_cast<char>(((ucs4 >> 0) & 0x3F) | 0x80);
					return result;
				}
				return result;
			}
			// https://ja.wikipedia.org/wiki/UTF-16
			static bool ucs2_to_ucs4(uint16_t first, uint16_t second, uint32_t& ucs4)
			{
				if(!is_surrogate_pair_first(first)) return false;
				if(!is_surrogate_pair_second(second)) return false;
				uint32_t w = (first >> 6) & 0xF;
				uint32_t x = (static_cast<uint32_t>(first & 0x3F) << 10) | static_cast<uint32_t>(second & 0x3FF);
				uint32_t u = w + 1;
				ucs4 = (u << 16) | x;
				return true;
			}
			static bool ucs4_to_utf16_surrogate_pair(uint32_t ucs4, uint16_t& first, uint16_t& second)
			{
				if(ucs4 <= 0xFFFF || 0x10FFFF < ucs4) return false;
				uint32_t x = ucs4 & 0xFFFF;
				uint32_t u = (ucs4 >> 16) & 0x1F;
				first = 0xD800 | ((u - 1) << 6) | ((x >> 10) & 0x3F);
				second = 0xDC00 | (x & 0x3FF);
				return true;
			}
			static bool is_surrogate_pair(uint16_t ucs2)
			{
				return 0xD800 <= ucs2 && ucs2 <= 0xDFFF;
			}
			static bool is_surrogate_pair_first(uint16_t ucs2)
			{
				return 0xD800 <= ucs2 && ucs2 <= 0xDBFF;
			}
			static bool is_surrogate_pair_second(uint16_t ucs2)
			{
				return 0xDC00 <= ucs2 && ucs2 <= 0xDFFF;
			}
			static bool hexdigits_to_ucs2(char in[4], uint16_t& ucs2)
			{
				ucs2 = 0;
				for(int i = 0; i < 4; ++i){
					int digit = hex_to_digit(in[i]);
					if(digit < 0) return false;
					ucs2 = (ucs2 << 4) | (static_cast<int16_t>(digit) & 0xf);
				}
				return true;
			}
			static int hex_to_digit(char c)
			{
				if('0' <= c && c <= '9') return static_cast<int>(c - '0');
				if('a' <= c && c <= 'f') return static_cast<int>(c - 'a' + 10);
				if('A' <= c && c <= 'F') return static_cast<int>(c - 'A' + 10);
				return -1;
			}
			static std::string escape(const std::string& in)
			{
				std::ostringstream oss;
				for(auto it = in.begin(), end = in.end(); it != end;){
					char c = *it;
					if(0 <= c){//U+0～U+7F
						++it;
						switch(c){
						case '"':
							oss << "\\\"";
							continue;
						case '\\':
							oss << "\\\\";
							continue;
							//case '/':
							//	oss << "\\/";
							//	continue;
						case '\b':
							oss << "\\b";
							continue;
						case '\f':
							oss << "\\f";
							continue;
						case '\n':
							oss << "\\n";
							continue;
						case '\r':
							oss << "\\r";
							continue;
						case '\t':
							oss << "\\t";
							continue;
						}
						if(c < 0x20){
							oss << "\\u" << std::setfill('0') << std::setw(4) << std::hex << static_cast<uint16_t>(c);
							continue;
						}
						oss << c;
						continue;
					}
					std::string utf8;
					uint8_t u = static_cast<uint8_t>(c);
					uint32_t ucs4 = 0;
					size_t follow = 0;
					if((u & 0xE0) == 0xC0){
						follow = 1;
						ucs4 = u & 0x1F;
						utf8 += c;
					} else if((u & 0xF0) == 0xE0){
						follow = 2;
						ucs4 = u & 0x0F;
						utf8 += c;
					} else if((u & 0xF8) == 0xF0){
						follow = 3;
						ucs4 = u & 0x07;
						utf8 += c;
					} else{
						return std::string();
					}
					++it;
					for(size_t i = 0; i < follow; ++i){
						if(it == end) return std::string();
						c = *it;
						u = static_cast<uint8_t>(c);
						if((u & 0xC0) != 0x80){
							return std::string();
						}
						ucs4 <<= 6;
						ucs4 |= u & 0x3F;
						utf8 += c;
						++it;
					}
					if(ucs4 < 0x10000){
						oss << utf8;
						//oss << "\\u" << std::setfill('0') << std::setw(4) << std::hex << static_cast<uint16_t>(ucs4);
					} else{
						uint16_t first, second;
						if(!ucs4_to_utf16_surrogate_pair(ucs4, first, second)) return std::string();
						oss << "\\u" << std::setfill('0') << std::setw(4) << std::hex << static_cast<uint16_t>(first);
						oss << "\\u" << std::setfill('0') << std::setw(4) << std::hex << static_cast<uint16_t>(second);
					}
				}
				return oss.str();
			}
		};
		class input {
		public:
			virtual ~input(){}
			virtual bool eof() const = 0;
			virtual size_t size() const = 0;
			virtual char get() = 0;
			virtual char next() = 0;
			virtual bool advance(size_t count) = 0;
			virtual bool fetch(char buf[], size_t length) = 0;
			virtual bool read(char buf[], size_t length) = 0;
		};
		class input_string : public input {
			std::string data;
			size_t offset;
			const char * ptr() const { return data.data() + offset; }
		public:
			input_string(const std::string & data)
				: data(data)
				, offset(0)
			{
			}
			input_string(std::string && data)
				: data(std::move(data))
				, offset(0)
			{
			}
			virtual ~input_string(){}
			virtual bool eof() const { return data.size() <= offset; }
			virtual size_t size() const { return data.size() - offset; }
			virtual bool advance(size_t count)
			{
				if(count <= size()){
					offset += count;
					return true;
				}
				return false;
			}
			virtual char get()
			{
				if(advance(1)){
					return ptr()[-1];
				}
				return '\0';
			}
			virtual char next()
			{
				if(size()){
					return ptr()[0];
				}
				return '\0';
			}
			virtual bool fetch(char * buf, size_t length)
			{
				if(length <= size()){
					memcpy(buf, ptr(), length);
					return true;
				}
				return false;
			}
			virtual bool read(char * buf, size_t length)
			{
				if(advance(length)){
					memcpy(buf, ptr() - length, length);
					return true;
				}
				return false;
			}
		};
		class output{
		public:
			virtual ~output(){}
			virtual bool out(const std::string& string) = 0;
		};
		class output_string : public output{
			std::ostringstream oss;
		public:
			virtual ~output_string(){}
			virtual bool out(const std::string& string)
			{
				oss << string;
				return true;
			}
			std::string get() { return oss.str(); }
		};
		class handler_interface{
		public:
			virtual bool start_document(){ return true; }
			virtual bool end_document(){ return true; }
			virtual bool start_array(){ return true; }
			virtual bool end_array(size_t /*count*/){ return true; }
			virtual bool start_object(){ return true; }
			virtual bool on_key(const std::string& /*key*/){ return true; }
			virtual bool end_object(size_t /*count*/){ return true; }
			virtual bool start_value(){ return true; }
			virtual bool end_value(){ return true; }
			virtual bool start_member(size_t /*index*/){ return true; }
			virtual bool end_member(){ return true; }
			virtual bool on_string(const std::string& /*string*/){ return true; }
			virtual bool on_null(){ return true; }
			virtual bool on_true(){ return true; }
			virtual bool on_false(){ return true; }
			virtual bool on_number(const std::string& /*number*/){ return true; }
			virtual bool on_number(double /*number*/){ return true; }
		};
		class output_handler : public handler_interface{
			output& out;
		public:
			output_handler(output& out)
				: out(out)
			{
			}
			bool eof() const { return true; }
			virtual bool start_member(size_t index)
			{
				if(index) out.out(",");
				return true;
			}
			virtual bool start_array()
			{
				out.out("[");
				return true;
			}
			virtual bool end_array(size_t count)
			{
				out.out("]");
				return true;
			}
			virtual bool start_object()
			{
				out.out("{");
				return true;
			}
			virtual bool end_object(size_t count)
			{
				out.out("}");
				return true;
			}
			virtual bool on_key(const std::string& key)
			{
				out.out("\"");
				out.out(unicode_tools::escape(key));
				out.out("\":");
				return true;
			}
			virtual bool on_string(const std::string& string)
			{
				out.out("\"");
				out.out(unicode_tools::escape(string));
				out.out("\"");
				return true;
			}
			virtual bool on_null()
			{
				out.out("null");
				return true;
			}
			virtual bool on_true()
			{
				out.out("true");
				return true;
			}
			virtual bool on_false()
			{
				out.out("false");
				return true;
			}
			virtual bool on_number(const std::string& number)
			{
				out.out(number);
				return true;
			}
		};
		enum value_types{
			initial_type, ///< for private use
			null_type,
			false_type,
			true_type,
			number_type,
			string_type,
			object_type,
			array_type,
		};
		class json_value{
		public:
			typedef std::deque<std::shared_ptr<json_value> > json_array;
			typedef std::map<std::string, std::shared_ptr<json_value> > json_object;
		private:
			value_types type;
			std::string string; // except for object, array
			json_array array; // array and key
			json_object object; // object
		public:
			json_value()
				: type(initial_type)
			{
			}
			json_value(const json_value& rhs)
				: type(rhs.type)
				, string(rhs.string)
			{
				for(auto it = rhs.array.begin(); it != rhs.array.end(); ++it){
					array.push_back((*it)->clone());
				}
				for(auto it = rhs.object.begin(); it != rhs.object.end(); ++it){
					object[it->first] = it->second->clone();
				}
			}
			json_value(const std::string& string)
				: type(string_type)
				, string(string)
			{
			}
			json_value(bool b)
				: type(initial_type)
			{
				if(b){
					set_true();
				}else{
					set_false();
				}
			}
			static std::shared_ptr<json_value> create_null()
			{
				auto value = std::make_shared<json_value>();
				value->set_null();
				return value;
			}
			bool operator==(const json_value& rhs) const { return equal(rhs); }
			bool operator<(const json_value& rhs) const { return compare(rhs) < 0; }
			bool operator>(const json_value& rhs) const { return compare(rhs) > 0; }
			bool operator<=(const json_value& rhs) const { return compare(rhs) <= 0; }
			bool operator>=(const json_value& rhs) const { return compare(rhs) >= 0; }
			std::shared_ptr<json_value> clone() const
			{
				std::shared_ptr<json_value> result = std::make_shared<json_value>();
				switch(type){
				case null_type:
					if(!result->set_null()) return false;
					break;
				case true_type:
					if(!result->set_true()) return false;
					break;
				case false_type:
					if(!result->set_false()) return false;
					break;
				case number_type:
					if(!result->set_number(string)) return false;
					break;
				case string_type:
					if(!result->set_string(string)) return false;
					break;
				case array_type:
				{
					if(!result->set_array()) return false;
					for(auto it = begin_array(), end = end_array(); it != end; ++it){
						if(!*it) return false;
						if(!result->append_to_array((*it)->clone())) return false;
					}
					break;
				}
				case object_type:
				{
					if(!result->set_object()) return false;
					for(auto it = begin_key(), end = end_key(); it != end; ++it){
						auto key_ptr = *it;
						if(!key_ptr || !key_ptr->is_string()) return false;
						auto key = key_ptr->get_string();
						auto value = get_member(key);
						if(!value) return false;
						if(!result->append_to_object(key, value->clone())) return false;
					}
					break;
				}
				}
				return result;
			}
			bool equal(const json_value& rhs) const
			{
				if(type != rhs.type) {
					return false;
				}
				switch(type){
				case number_type:
					if(string != rhs.string){
						return false;
					}
					return true;
				case string_type:
					if(string != rhs.string){
						return false;
					}
					return true;
				case array_type:
				{
					if(array.size() != rhs.array.size()) {
						return false;
					}
					for(auto it = begin_array(), end = end_array(), rit = rhs.begin_array(); it != end; ++it, ++rit){
						if(!*it || !*rit) {
							return false;
						}
						if(!(*it)->equal(**rit)) {
							return false;
						}
					}
					break;
				}
				case object_type:
				{
					if(object.size() != rhs.object.size()) {
						return false;
					}
					for(auto it = begin_key(), end = end_key(); it != end; ++it){
						if(!*it || !(*it)->is_string()) {
							return false;
						}
						auto key = (*it)->get_string();
						auto value = get_member(key);
						if(!value) {
							return false;
						}
						auto rhs_value = rhs.get_member(key);
						if(!rhs_value) {
							return false;
						}
						if(!value->equal(*rhs_value)) {
							return false;
						}
					}
					break;
				}
				}
				return true;
			}
		private:
			template<typename T>
			static int compare_type(T lhs, T rhs)
			{
				if(lhs == rhs) return 0;
				if(lhs < rhs) return -1;
				return 1;
			}
		public:
			int compare(const json_value& rhs) const
			{
				if(type != rhs.type) return compare_type(static_cast<int>(type), static_cast<int>(rhs.type));
				switch(type){
				case number_type:
					return compare_type(string, rhs.string);
				case string_type:
					return compare_type(string, rhs.string);
				case array_type:
				{
					if(array.size() != rhs.array.size()) return compare_type(array.size(), rhs.array.size());
					for(auto it = begin_array(), end = end_array(), rit = rhs.begin_array(); it != end; ++it, rit){
						if(!*it) return -1;
						if(!*rit) return 1;
						auto r = (*it)->compare(**rit);
						if(r) return r;
					}
					break;
				}
				case object_type:
				{
					if(object.size() != rhs.object.size()) return compare_type(object.size(), rhs.object.size());
					if(array.size() != rhs.array.size()) return compare_type(array.size(), rhs.array.size());
					for(auto it = begin_key(), end = end_key(), rit = rhs.begin_key(); it != end; ++it, ++rit){
						if(!*it || (*it)->is_string()) return -1;
						if(!*rit || (*rit)->is_string()) return 1;
						auto key = (*it)->get_string();
						auto rkey = (*rit)->get_string();
						auto r = compare_type(key, rkey);
						if(r) return r;
						auto value = get_member(key);
						if(!value) return -1;
						auto rhs_value = rhs.get_member(rkey);
						if(!rhs_value) return 1;
						r = value->compare(*rhs_value);
						if(r) return r;
					}
					break;
				}
				default:
					return 0;
				}
			}
			value_types get_type() const { return type; }
			bool is_null() const { return type == null_type; }
			bool is_true() const { return type == true_type; }
			bool is_false() const { return type == false_type; }
			bool is_bool() const { return is_true() || is_false(); }
			bool is_number() const { return type == number_type; }
			bool is_string() const { return type == string_type; }
			bool is_object() const { return type == object_type; }
			bool is_array() const { return type == array_type; }
			bool is_primitive() const { return is_null() || is_bool() || is_number() || is_string(); }
			bool is_valid_type() const { return is_primitive() || is_object() || is_array(); }
			size_t size() const
			{
				if(is_array() || is_object()) return array.size();
				if(is_string()) return string.size();
				return 0;
			}
			bool get_bool() const { return is_true(); }
			std::string get_string() const { return is_string() ? string : std::string(); }
			std::string get_number() const { return is_number() ? string : std::string(); }
			std::shared_ptr<json_value> get_member(size_t index) const
			{
				if(!is_array()) return nullptr;
				if(array.size() <= index) return nullptr;
				return array[index];
			}
			std::shared_ptr<json_value> get_member(const std::string& key) const
			{
				if(!is_object()) return nullptr;
				auto it = object.find(key);
				if(it == object.end()) return nullptr;
				return it->second;
			}
			std::shared_ptr<json_value> operator[](size_t index) const
			{
				return get_member(index);
			}
			std::shared_ptr<json_value> operator[](const std::string& key) const
			{
				return get_member(key);
			}
			std::shared_ptr<json_value> slice(size_t start, size_t end, size_t step=1) const
			{
				std::shared_ptr<json_value> result = std::make_shared<json_value>();
				result->set_array();
				if(is_array()){
					for(size_t i = start; i < end && i < array.size();){
						result->append_to_array(array[i]);
						if(i + step <= i) break;
						i += step;
					}
				}
				return result;
			}
			bool erase_member(const std::string& key)
			{
				if(!is_object()) return false;
				auto it = object.find(key);
				if(it == object.end()) return false;
				for(auto it = array.begin(), end = array.end(); it != end; ++it){
					if(*it && (*it)->is_string() && (*it)->get_string() == key){
						array.erase(it);
						break;
					}
				}
				object.erase(key);
				if(array.size() != object.size()){
					return false;
				}
				return true;
			}
			bool erase_member(size_t index)
			{
				if(!is_array()) return false;
				if(array.size() <= index) return false;
				array.erase(array.begin() + index);
				return true;
			}
			bool set_null()
			{
				type = null_type;
				string = "null";
				return true;
			}
			bool set_true()
			{
				type = true_type;
				string = "true";
				return true;
			}
			bool set_false()
			{
				type = false_type;
				string = "false";
				return true;
			}
			bool set_number(const std::string& number)
			{
				double real;
				if(!json_parser::number_to_double(number, real)){
					return false;
				}
				type = number_type;
				string = number;
				return true;
			}
			bool set_string(const std::string& string)
			{
				type = string_type;
				this->string = string;
				return true;
			}
			bool set_object()
			{
				type = object_type;
				object.clear();
				array.clear();
				return true;
			}
			bool set_array()
			{
				type = array_type;
				array.clear();
				return true;
			}
			bool append_to_array(const std::shared_ptr<json_value>& value)
			{
				if(!value || !value->is_valid_type()) return false;
				if(type != array_type) return false;
				array.push_back(value);
				return true;
			}
			bool insert_to_array(size_t index, const std::shared_ptr<json_value>& value)
			{
				if(!value || !value->is_valid_type()) return false;
				if(type != array_type) return false;
				if(array.size() < index) return false;
				array.insert(array.begin() + index, value);
				return true;
			}
			bool set_to_array(size_t index, const std::shared_ptr<json_value>& value)
			{
				if(!value || !value->is_valid_type()) return false;
				if(type != array_type) return false;
				if(array.size() <= index) return false;
				array[index] = value;
				return true;
			}
			json_array::iterator begin_array() { return array.begin(); }
			json_array::iterator end_array() { return array.end(); }
			json_array::const_iterator begin_array() const { return array.begin(); }
			json_array::const_iterator end_array() const { return array.end(); }
			json_array::reverse_iterator rbegin_array() { return array.rbegin(); }
			json_array::reverse_iterator rend_array() { return array.rend(); }
			json_array::const_reverse_iterator rbegin_array() const { return array.rbegin(); }
			json_array::const_reverse_iterator rend_array() const { return array.rend(); }
			bool append_to_object(const std::string& key, const std::shared_ptr<json_value>& value)
			{
				if(!value || !value->is_valid_type()) return false;
				if(type != object_type) return false;
				if(object.find(key) != object.end()){
					object[key] = value;
					return true;
				}
				if(array.size() != object.size()) return false;
				array.push_back(std::make_shared<json_value>(key));
				object[key] = value;
				return true;
			}
			bool insert_to_object(size_t index, const std::string& key, const std::shared_ptr<json_value>& value)
			{
				if(!value || !value->is_valid_type()) return false;
				if(type != object_type) return false;
				if(object.find(key) != object.end()){
					object[key] = value;
					return true;
				}
				if(array.size() != object.size()) return false;
				if(array.size() < index) return false;
				array.insert(array.begin() + index, std::make_shared<json_value>(key));
				object[key] = value;
				return true;
			}
			json_array::iterator begin_key() { return array.begin(); }
			json_array::iterator end_key() { return array.end(); }
			json_array::const_iterator begin_key() const { return array.begin(); }
			json_array::const_iterator end_key() const { return array.end(); }
			json_array::reverse_iterator rbegin_key() { return array.rbegin(); }
			json_array::reverse_iterator rend_key() { return array.rend(); }
			json_array::const_reverse_iterator rbegin_key() const { return array.rbegin(); }
			json_array::const_reverse_iterator rend_key() const { return array.rend(); }
			bool serialize(handler_interface& handler) const
			{
				switch(type){
				case null_type:
					if(!handler.on_null()) return false;
					break;
				case true_type:
					if(!handler.on_true()) return false;
					break;
				case false_type:
					if(!handler.on_false()) return false;
					break;
				case number_type:
					if(!handler.on_number(string)) return false;
					break;
				case string_type:
					if(!handler.on_string(string)) return false;
					break;
				case object_type:
					{
						if(!handler.start_object()) return false;
						size_t index = 0;
						for(auto it = begin_key(), end = end_key(); it != end; ++it, ++index){
							if(!*it) return false;
							std::string key = (*it)->string;
							auto oit = object.find(key);
							if(oit == object.end()) return false;
							if(!oit->second) return false;
							if(!handler.start_member(index)) return false;
							if(!handler.on_key(key)) return false;
							if(!handler.start_value()) return false;
							if(!oit->second->serialize(handler)) return false;
							if(!handler.end_value()) return false;
							if(!handler.end_member()) return false;
						}
						if(!handler.end_object(object.size())) return false;
					}
					break;
				case array_type:
					if(!handler.start_array()) return false;
					for(size_t i = 0; i < array.size(); ++i){
						if(!handler.start_member(i)) return false;
						if(!handler.start_value()) return false;
						if(!array[i]->serialize(handler)) return false;
						if(!handler.end_value()) return false;
						if(!handler.end_member()) return false;
					}
					if(!handler.end_array(array.size())) return false;
					break;
				default:
					return false;
				}
				return true;
			}
			std::string to_str() const
			{
				output_string os;
				output_handler oh(os);
				if(!oh.start_document()) return std::string();
				if(!serialize(oh)) return std::string();
				if(!oh.end_document()) return std::string();
				return os.get();
			}
			double to_double() const
			{
				if(is_number()){
					double real = 0;
					if(json_parser::number_to_double(string, real)){
						return real;
					}
				}
				return 0;
			}
			template<typename T>
			T to_integer() const
			{
				if(is_number()){
					T integer = 0;
					if(json_parser::number_to_integer(string, integer)){
						return integer;
					}
				}
				return 0;
			}
			class data_handler : public handler_interface{
				struct stack_element{
					json_value * value;
					bool in_array;
					bool in_object;
					std::string last_key;
					stack_element(json_value * value = nullptr)
						: value(value)
						, in_array(false)
						, in_object(false)
						, last_key()
					{
					}
				};
				std::deque<stack_element> stack;
			public:
				data_handler(json_value* value)
					: stack()
				{
					stack.push_back(stack_element(value));
				}
				bool eof() const { return stack.empty(); }
				virtual bool start_document()
				{
					return stack.size() == 1;
				}
				virtual bool end_document()
				{
					if(stack.size() != 0) return false;
					return true;
				}
				virtual bool start_value()
				{
					if(stack.empty()) return false;
					auto& parent = stack.back();
					if(parent.in_array){
						parent.value->array.push_back(std::make_shared<json_value>());
						stack.push_back(stack_element(parent.value->array.back().get()));
					} else if(parent.in_object){
						parent.value->array.push_back(std::make_shared<json_value>(parent.last_key));
						parent.value->object[parent.last_key] = std::make_shared<json_value>();
						stack.push_back(stack_element(parent.value->object[parent.last_key].get()));
					}
					return true;
				}
				virtual bool end_value()
				{
					if(stack.empty()) return false;
					stack.pop_back();
					return true;
				}
				virtual bool start_array()
				{
					if(stack.empty()) return false;
					auto& parent = stack.back();
					if(parent.in_array || parent.in_object) return false;
					parent.in_array = true;
					return true;
				}
				virtual bool end_array(size_t count)
				{
					if(stack.empty()) return false;
					auto& parent = stack.back();
					if(!parent.in_array) return false;
					if(parent.value->array.size() != count) return false;
					parent.value->type = array_type;
					return true;
				}
				virtual bool start_object()
				{
					if(stack.empty()) return false;
					auto& parent = stack.back();
					if(parent.in_array || parent.in_object) return false;
					parent.in_object = true;
					return true;
				}
				virtual bool end_object(size_t count)
				{
					if(stack.empty()) return false;
					auto& parent = stack.back();
					if(!parent.in_object) return false;
					if(parent.value->object.size() != count) return false;
					parent.value->type = object_type;
					return true;
				}
				virtual bool on_key(const std::string& key)
				{
					if(stack.empty()) return false;
					auto& parent = stack.back();
					json_value * value = parent.value;
					if(!parent.in_object) return false;
					// RFC-8259 require names SHOULD be unique.
					if(value->object.find(key) != value->object.end()) return false; // duplicated
					parent.last_key = key;
					return true;
				}
				virtual bool on_string(const std::string& string)
				{
					if(stack.empty()) return false;
					auto& parent = stack.back();
					json_value * value = parent.value;
					value->string = string;
					value->type = string_type;
					return true;
				}
				virtual bool on_null()
				{
					if(stack.empty()) return false;
					auto& parent = stack.back();
					json_value * value = parent.value;
					value->string = "null";
					value->type = null_type;
					return true;
				}
				virtual bool on_true()
				{
					if(stack.empty()) return false;
					auto& parent = stack.back();
					json_value * value = parent.value;
					value->string = "true";
					value->type = true_type;
					return true;
				}
				virtual bool on_false()
				{
					if(stack.empty()) return false;
					auto& parent = stack.back();
					json_value * value = parent.value;
					value->string = "false";
					value->type = false_type;
					return true;
				}
				virtual bool on_number(const std::string& number)
				{
					if(stack.empty()) return false;
					auto& parent = stack.back();
					json_value * value = parent.value;
					value->string = number;
					value->type = number_type;
					return true;
				}
			};
			class json_parser{
				handler_interface & handler;
			public:
				json_parser(handler_interface & handler)
					: handler(handler)
				{
				}
				bool parse(input& in)
				{
					if(!handler.start_document()) return false;
					//RFC-4627 JSON-text = object / array
					//RFC-8259 JSON-text = ws value ws
					parse_whitespace(in);
					if(!parse_value(in, 0)){
						return false;
					}
					parse_whitespace(in);
					if(!handler.end_document()) return false;
					return true;
				}
				bool check_structural_character(input& in, char c)
				{
					parse_whitespace(in);
					if(in.next() != c) return false;
					parse_whitespace(in);
					return true;
				}
				bool parse_structural_character(input& in, char c)
				{
					parse_whitespace(in);
					if(in.get() != c) return false;
					parse_whitespace(in);
					return true;
				}
				bool parse_object(input& in)
				{
					if(!parse_structural_character(in, '{')) return false;// no object format
					size_t count = 0;
					if(!handler.start_object()) return false;
					while(!check_structural_character(in, '}')){
						if(!handler.start_member(count)) return false;
						if(count){
							if(!parse_structural_character(in, ',')) return false;// no separater
						}
						std::string key;
						if(!parse_string(in, key)) return false; // no key string
						if(!handler.on_key(key)) return false;
						if(!parse_structural_character(in, ':')) return false;// no separater
						if(!parse_value(in, count)) return false; // no value
						if(!handler.end_member()) return false;
						++count;
					}
					if(!parse_structural_character(in, '}')) return false;// object terminate error
					if(!handler.end_object(count)) return false;
					return true;
				}
				bool parse_array(input& in)
				{
					if(!parse_structural_character(in, '[')) return false;// no array format
					size_t count = 0;
					if(!handler.start_array()) return false;
					while(!check_structural_character(in, ']')){
						if(!handler.start_member(count)) return false;
						if(count){
							if(!parse_structural_character(in, ',')) return false;// no separater
						}
						if(!parse_value(in, count)) return false; // no value
						if(!handler.end_member()) return false;
						++count;
					}
					if(!parse_structural_character(in, ']')) return false;// array terminate error
					if(!handler.end_array(count)) return false;
					return true;
				}
				bool parse_value(input& in, size_t index)
				{
					if(!handler.start_value()) return false;
					char buf[5];
					std::string s;
					switch(in.next()){
					case '{':
						if(!parse_object(in)) return false;
						break;
					case '[':
						if(!parse_array(in)) return false;
						break;
					case '"':
						if(!parse_string(in, s)) return false;
						if(!handler.on_string(s)) return false;
						break;
					case 't':
						if(!in.read(buf, 4)) return false;// insufficiency
						if(memcmp(buf, "true", 4) != 0) return false; // no true
						if(!handler.on_true()) return false;
						break;
					case 'f':
						if(!in.read(buf, 5)) return false;// insufficiency
						if(memcmp(buf, "false", 5) != 0) return false; // no false
						if(!handler.on_false()) return false;
						break;
					case 'n':
						if(!in.read(buf, 4)) return false;// insufficiency
						if(memcmp(buf, "null", 4) != 0) return false; // no null
						if(!handler.on_null()) return false;
						break;
					default:
						if(!parse_number(in, s)) return false;
						if(!handler.on_number(s)) return false;
						break;
					}
					if(!handler.end_value()) return false;
					return true;
				}
				static bool number_to_double(const std::string& number, double& real)
				{
					auto it = number.begin();
					auto end = number.end();
					real = 0;
					if(it == end) return false;
					// sign
					bool minus = false;
					if(*it == '-'){
						++it;
						minus = true;
					}
					if(it == end) return false;
					// integer
					if(*it == '0'){
						++it;
					} else if('1' <= *it && *it <= '9'){
						while(it != end && '0' <= *it && *it <= '9'){
							real *= 10;
							real += (*it) - '0';
							++it;
						}
					} else{
						return false;
					}
					// fraction
					if(it != end && *it == '.'){
						if(++it == end) return false;
						double factor = 1;
						while(it != end && '0' <= *it && *it <= '9'){
							factor /= 10;
							real += ((*it) - '0') * factor;
							++it;
						}
						if(factor == 1) return false;
					}
					if(minus) real *= -1;
					// exponent
					if(it != end && (*it == 'e' || *it == 'E')){
						if(++it == end) return false;
						minus = (*it == '-');
						if(*it == '+' || *it == '-'){
							if(++it == end) return false;
						}
						double exponent = 0;
						bool any_exponent = false;
						while(it != end && '0' <= *it && *it <= '9'){
							exponent *= 10;
							exponent += *it - '0';
							any_exponent = true;
						}
						if(!any_exponent) return false;
						if(minus) exponent *= -1;
						real *= pow(10, exponent);
					}
					if(it != end) return false;
					return true;
				}
				template<typename T>
				static bool number_to_integer(const std::string& number, T& integer)
				{
					auto it = number.begin();
					auto end = number.end();
					integer = 0;
					if(it == end) return false;
					// sign
					bool minus = false;
					if(*it == '-'){
						++it;
						minus = true;
					}
					if(it == end) return false;
					// integer
					if(*it == '0'){
						++it;
					} else if('1' <= *it && *it <= '9'){
						while(it != end && '0' <= *it && *it <= '9'){
							if(std::numeric_limits<T>::max() / 10 < integer){
								return false;
							}
							integer *= 10;
							if(std::numeric_limits<T>::max() - ((*it) - '0') < integer){
								return false;
							}
							integer += (*it) - '0';
							++it;
						}
					} else{
						return false;
					}
					// fraction
					if(it != end && *it == '.'){
						return false;
					}
					if(minus) {
						if(std::numeric_limits<T>::is_signed()){
							if(integer){
								return false;
							}
						}
						integer *= -1;
					}
					// exponent
					if(it != end && (*it == 'e' || *it == 'E')){
						return false;
					}
					if(it != end) return false;
					return true;
				}
			private:
				static bool parse_string(input& in, std::string& string)
				{
					std::string result;
					if(in.get() != '"') return false;// no string format
					char c;
					if(!in.fetch(&c, 1)) return false;// insufficiency
					while(c != '"'){
						// escape
						if(c == '\\'){
							result += c;
							in.advance(1);
							switch(in.get()){
							case '"':
								result += '"';
								break;
							case '\\':
								result += '\\';
								break;
							case '/':
								result += '/';
								break;
							case 'b':
								result += 'b';
								break;
							case 'f':
								result += 'f';
								break;
							case 'n':
								result += 'n';
								break;
							case 'r':
								result += 'r';
								break;
							case 't':
								result += 't';
								break;
							case 'u':
							{
								result += 'u';
								char hexdigits[5] = {};
								if(!in.fetch(hexdigits, 4)) return false;// insufficiency
								in.advance(4);
								uint16_t ucs2;
								if(!unicode_tools::hexdigits_to_ucs2(hexdigits, ucs2)) return false;
								uint32_t ucs4 = ucs2;
								result += hexdigits;
								if(unicode_tools::is_surrogate_pair(ucs2)){
									if(in.get() != '\\') return false;// illegal format for followed surrogate pair
									if(in.get() != 'u') return false;// illegal format for followed surrogate pair
									if(!in.read(hexdigits, 4)) return false;// illegal format for followed surrogate pair
									uint16_t second;
									if(!unicode_tools::hexdigits_to_ucs2(hexdigits, second)) return false;
									if(!unicode_tools::ucs2_to_ucs4(ucs2, second, ucs4)) return false;// illegal surrogate pair
									result += "\\u";
									result += hexdigits;
								}
								break;
							}
							default:
								return false;// illegal escape
							}
						} else{
							result += in.get();
						}
						if(!in.fetch(&c, 1)) return false;// insufficiency
					}
					if(in.get() != '"') return false;// string terminate error
					string = unescape_json_string(result);
					if(!result.empty() && string.empty()) return false;
					return true;
				}
				static bool parse_number(input& in, std::string& number)
				{
					std::string result;
					char c;
					if(!in.fetch(&c, 1)) return false;
					// sign
					if(c == '-'){
						result += in.get();
						if(!in.fetch(&c, 1)) return false;
					}
					// integer
					if(c == '0'){
						result += in.get();
						if(!in.fetch(&c, 1)) return false;
					} else if('1' <= c && c <= '9'){
						if(!parse_digits(in, result)) return false;
						if(!in.fetch(&c, 1)) return false;
					} else{
						return false;
					}
					// fraction
					if(c == '.'){
						result += in.get();
						if(!in.fetch(&c, 1)) return false;
						if(!parse_digits(in, result)) return false;
						if(!in.fetch(&c, 1)) return false;
					}
					// exponent
					if(c == 'e' || c == 'E'){
						result += in.get();
						if(!in.fetch(&c, 1)) return false;
						if(c == '+' || c == '-'){
							result += in.get();
						}
						if(!parse_digits(in, result)) return false;
					}
					number = result;
					return true;
				}
				static bool parse_in_range(input& in, std::string& out, char first, char last)
				{
					if(in.eof()) return false;
					char c = in.next();
					if(first <= c && c <= last){
						out += in.get();
						return true;
					}
					return false;
				}
				static bool parse_digits(input& in, std::string& out)
				{
					bool any_digit = false;
					while(parse_digit(in, out)){
						any_digit = true;
					}
					return any_digit;
				}
				static bool parse_digit(input& in, std::string& out)
				{
					return parse_in_range(in, out, '0', '9');
				}
				static bool parse_digit19(input& in, std::string& out)
				{
					return parse_in_range(in, out, '1', '9');
				}
				static void parse_whitespace(input& in)
				{
					while(!in.eof()){
						switch(in.next()){
						case ' ':
						case '\r':
						case '\n':
						case '\t':
							in.advance(1);
							continue;
						}
						break;
					}
				}
				static std::string unescape_json_string(const std::string& string)
				{
					std::string result;
					for(auto it = string.begin(), end = string.end(); it != end; ++it){
						char c = *it;
						// control characters
						if(0 <= c && c < ' '){
							return std::string();// illegal control characters
						}
						// escape
						if(c == '\\'){
							++it;
							if(it == end) return std::string();
							switch(*it){
							case '"':
								result += '"';
								break;
							case '\\':
								result += '\\';
								break;
							case '/':
								result += '/';
								break;
							case 'b':
								result += '\b';
								break;
							case 'f':
								result += '\f';
								break;
							case 'n':
								result += '\n';
								break;
							case 'r':
								result += '\r';
								break;
							case 't':
								result += '\t';
								break;
							case 'u':
							{
								char hexdigits[4];
								for(int i = 0; i < 4; ++i){
									if(++it == end) return std::string();
									hexdigits[i] = *it;
								}
								uint16_t ucs2;
								if(!unicode_tools::hexdigits_to_ucs2(hexdigits, ucs2)) return std::string();
								uint32_t ucs4 = ucs2;
								if(unicode_tools::is_surrogate_pair(ucs2)){
									uint16_t first = ucs2;
									if(++it == end || *it != '\\') return std::string();// illegal format for followed surrogate pair
									if(++it == end || *it != 'u') return std::string();// illegal format for followed surrogate pair
									for(int i = 0; i < 4; ++i){
										if(++it == end) return std::string();
										hexdigits[i] = *it;
									}
									uint16_t second;
									if(!unicode_tools::hexdigits_to_ucs2(hexdigits, second)) return std::string();
									if(!unicode_tools::ucs2_to_ucs4(first, second, ucs4)) return std::string();// illegal surrogate pair
								}
								result += unicode_tools::ucs4_to_utf8(ucs4);
								break;
							}
							default:
								return std::string();// illegal escape
							}
						} else{
							result += *it;
						}
					}
					return result;
				}
			};
			static std::shared_ptr<json_value> parse(const std::string& in)
			{
				std::shared_ptr<json_value> result = std::make_shared<json_value>();
				data_handler dh(result.get());
				json_parser jp(dh);
				input_string is(in);
				if(!jp.parse(is)) return nullptr;
				if(!dh.eof()) return nullptr;
				return result;
			}
			static std::shared_ptr<json_value> parse_partial(std::string& in)
			{
				std::shared_ptr<json_value> result = std::make_shared<json_value>();
				data_handler dh(result.get());
				json_parser jp(dh);
				input_string is(in);
				if(!jp.parse(is)) return nullptr;
				if(!dh.eof()) return nullptr;
				if(in.size() >= is.size()){
					in = in.substr(in.size() - is.size());
					return result;
				}
				return nullptr;
			}
			// https://tools.ietf.org/html/rfc7396
			static std::shared_ptr<json_value> merge_patch(std::shared_ptr<json_value> target, const std::shared_ptr<json_value>& patch)
			{
				if(!patch) {
					return nullptr;
				}
				if(patch->is_object()){
					if(!target){
						target = std::make_shared<json_value>();
					}
					if(!target->is_object()){
						if(!target->set_object()) {
							return nullptr;
						}
					}
					for(auto it = patch->begin_key(), end = patch->end_key(); it != end; ++it){
						auto& key_ptr = *it;
						if(!key_ptr || !key_ptr->is_string()) {
							return nullptr;
						}
						auto key = key_ptr->get_string();
						auto value_ptr = patch->get_member(key);
						if(!value_ptr) {
							return nullptr;
						}
						if(value_ptr->is_null()){
							target->erase_member(key);
						}else{
							auto target_value_ptr = target->get_member(key);
							if(!target->append_to_object(key, merge_patch(target_value_ptr, value_ptr))) {
								return nullptr;
							}
						}
					}
					return target;
				}else{
					return patch;
				}
			}
			// https://tools.ietf.org/html/rfc6901
			class json_pointer{
			public:
				std::vector<std::string> tokens;
				std::shared_ptr<json_value> target;
				bool parse(const std::string& pointer)
				{
					for(size_t offset = 0; offset < pointer.size(); ){
						if(pointer[offset] != '/') return false;
						++offset;
						size_t separater = pointer.find('/', offset);
						std::string token;
						if(separater != std::string::npos){
							token = pointer.substr(offset, separater - offset);
							if(!decode(token)) return false;
							tokens.push_back(token);
							offset = separater;
						} else{
							token = pointer.substr(offset);
							if(!decode(token)) return false;
							tokens.push_back(token);
							break;
						}
					}
					return true;
				}
				bool decode(std::string& token)
				{
					if(token.find('~') == std::string::npos){
						return true;
					}
					std::ostringstream oss;
					for(auto it = token.begin(), end = token.end(); it != end; ++it){
						if(*it == '~'){
							++it;
							if(it == end) return false;
							if(*it == '0'){
								oss << "~";
							} else if(*it == '1'){
								oss << "/";
							} else{
								return false;
							}
						} else{
							oss << *it;
						}
					}
					token = oss.str();
					return true;
				}
				virtual bool on_object(std::shared_ptr<json_value>, const std::string&)
				{
					return false;
				}
				virtual bool on_array(std::shared_ptr<json_value>, size_t index)
				{
					return false;
				}
				virtual bool on_root(std::shared_ptr<json_value>&)
				{
					return false;
				}
				virtual bool on_error(std::shared_ptr<json_value>, const std::string&)
				{
					return false;
				}
				virtual bool on_error(std::shared_ptr<json_value>, size_t)
				{
					return false;
				}
				bool evaluate(std::shared_ptr<json_value>& target)
				{
					this->target = target;
					if(!target){
						return false;
					}
					std::shared_ptr<json_value> current = target;
					if(tokens.empty()){
						return on_root(target);
					}
					for(auto it = tokens.begin(), end = tokens.end(); it != end && current; ++it){
						const auto& token = *it;
						const bool last = (it + 1 == end);
						switch(current->type){
						case array_type:
						{
							if(token == "-"){
								if(last){
									return on_array(current, current->array.size());
								}
								return on_error(current, current->array.size());
							}
							char* endptr = nullptr;
							auto index = strtoull(token.c_str(), &endptr, 10);
							if(index == ULLONG_MAX && errno == ERANGE) return false; // out of range
							if(endptr && endptr != token.c_str() + token.size()) return false; // invalid format
							if(current->array.size() < index){
								return on_error(current, index);
							}
							if(last){
								return on_array(current, index);
							}
							if(current->array.size() == index){
								return on_error(current, index);
							}
							current = current->array[index];
						}
						break;
						case object_type:
						{
							if(last){
								return on_object(current, token);
							}
							auto it = current->object.find(token);
							if(it == current->object.end()){
								if(!on_error(current, token)){
									return false;
								}
								it = current->object.find(token);
								if(it == current->object.end()){
									return false;
								}
							}
							current = it->second;
						}
						break;
						default:
							return false;
						}
					}
					if(!current){
						return false;
					}
					return true;
				}
			};
			// https://tools.ietf.org/html/rfc6902
			class json_patch_add : public json_pointer{
				std::shared_ptr<json_value> value;
			public:
				json_patch_add(std::shared_ptr<json_value> value)
				: value(value)
				{
				}
				virtual bool on_object(std::shared_ptr<json_value> parent, const std::string& key)
				{
					if(!parent) return false;
					if(!value) return false;
					return parent->append_to_object(key, value);
				}
				virtual bool on_array(std::shared_ptr<json_value> parent, size_t index)
				{
					if(!parent) return false;
					if(!value) return false;
					return parent->insert_to_array(index, value);
				}
				virtual bool on_root(std::shared_ptr<json_value>& root)
				{
					if(!root) return false;
					if(!value) return false;
					root = value;
					return true;
				}
				virtual bool on_error(std::shared_ptr<json_value> parent, const std::string& key)
				{
					if(parent == target){
						return false;
					}
					if(!parent->append_to_object(key, std::make_shared<json_value>())){
						return false;
					}
					return true;
				}
			};
			class json_patch_get : public json_pointer{
				std::shared_ptr<json_value> value;
			public:
				json_patch_get()
				{
				}
				std::shared_ptr<json_value> get() { return value; }
				virtual bool on_object(std::shared_ptr<json_value> parent, const std::string& key)
				{
					if(!parent) return false;
					value = parent->get_member(key);
					if(!value) return false;
					return true;
				}
				virtual bool on_array(std::shared_ptr<json_value> parent, size_t index)
				{
					if(!parent) return false;
					value = parent->get_member(index);
					if(!value) return false;
					return true;
				}
			};
			class json_patch_remove : public json_pointer{
				std::shared_ptr<json_value> value;
			public:
				json_patch_remove()
				{
				}
				std::shared_ptr<json_value> get() { return value; }
				virtual bool on_object(std::shared_ptr<json_value> parent, const std::string& key)
				{
					if(!parent) return false;
					value = parent->get_member(key);
					return parent->erase_member(key);
				}
				virtual bool on_array(std::shared_ptr<json_value> parent, size_t index)
				{
					if(!parent) return false;
					value = parent->get_member(index);
					return parent->erase_member(index);
				}
			};
			class json_patch_replace : public json_pointer{
				std::shared_ptr<json_value> value;
			public:
				json_patch_replace(std::shared_ptr<json_value> value)
					: value(value)
				{
				}
				virtual bool on_object(std::shared_ptr<json_value> parent, const std::string& key)
				{
					if(!parent) return false;
					if(!value) return false;
					if(!parent->get_member(key)) return false;
					return parent->append_to_object(key, value);
				}
				virtual bool on_array(std::shared_ptr<json_value> parent, size_t index)
				{
					if(!parent) return false;
					if(!value) return false;
					return parent->set_to_array(index, value);
				}
			};
			class json_patch_test : public json_pointer{
				std::shared_ptr<json_value> value;
			public:
				json_patch_test(std::shared_ptr<json_value> value)
				: value(value)
				{
				}
				virtual bool on_object(std::shared_ptr<json_value> parent, const std::string& key)
				{
					if(!parent) return false;
					auto member = parent->get_member(key);
					if(!member || !value) return false;
					return value->equal(*member);
				}
				virtual bool on_array(std::shared_ptr<json_value> parent, size_t index)
				{
					if(!parent) return false;
					auto member = parent->get_member(index);
					if(!member || !value) return false;
					return value->equal(*member);
				}
				static std::string string_unsigned_integer_add(const std::string& lhs, const std::string& rhs)
				{
					if(lhs.empty() || rhs.empty()) return std::string();
					std::string result;
					auto lit = lhs.rbegin(), lend = lhs.rend();
					auto rit = rhs.rbegin(), rend = rhs.rend();
					int digit = 0;
					while(lit != lend && rit != rend){
						digit += (*lit - '0') + (*rit - '0');
						result.push_back('0' + (digit % 10));
						digit /= 10;
						++lit;
						++rit;
					}
					while(lit != lend){
						digit += (*lit - '0');
						result.push_back('0' + (digit % 10));
						digit /= 10;
						++lit;
					}
					while(rit != rend){
						digit += (*rit - '0');
						result.push_back('0' + (digit % 10));
						digit /= 10;
						++rit;
					}
					if(digit){
						result.push_back('0' + (digit % 10));
					}
					std::reverse(result.begin(), result.end());
					return result;
				}
				static int string_unsigned_integer_compare(const std::string& lhs, const std::string& rhs)
				{
					if(lhs.size() != rhs.size()) {
						if(lhs.size() < rhs.size()) return -1;
						return 1;
					}
					for(auto lit = lhs.begin(), lend = lhs.end(), rit = rhs.begin(); lit != lend; ++lit, ++rit){
						if(*lit != *rit){
							if(*lit < *rit) return -1;
							return 1;
						}
					}
					return 0;
				}
				static std::string string_unsigned_integer_minus(const std::string& lhs, const std::string& rhs)
				{
					if(lhs.empty() || rhs.empty()) return std::string();
					int comp = string_unsigned_integer_compare(lhs, rhs);
					if(comp == 0) return std::string("0");
					if(comp < 0){ // lhs < rhs
						return std::string("-") + string_unsigned_integer_minus(rhs, lhs);
					}
					std::string result;
					auto lit = lhs.rbegin(), lend = lhs.rend();
					auto rit = rhs.rbegin(), rend = rhs.rend();
					int digit = 0;
					while(lit != lend && rit != rend){
						digit += (*lit - '0') - (*rit - '0');
						result.push_back('0' + ((10 + digit) % 10));
						if(digit < 0){
							digit = -1;
						}else{
							digit = 0;
						}
						++lit;
						++rit;
					}
					while(lit != lend && digit){
						digit += (*lit - '0');
						result.push_back('0' + ((10 + digit) % 10));
						if(digit < 0){
							digit = -1;
						} else{
							digit = 0;
						}
						++lit;
					}
					while(lit != lend){
						result.push_back(*lit);
						++lit;
					}
					if(rit != rend || digit){
						return std::string("0");//error
					}
					while(!result.empty() && result.back() == '0'){
						result.pop_back();
					}
					std::reverse(result.begin(), result.end());
					return result;
				}
				static std::string string_signed_integer_add(const std::string& lhs, const std::string& rhs)
				{
					if(lhs.empty() || rhs.empty()) return std::string();
					if(rhs == "0") return lhs;
					if(lhs == "0") return rhs;
					bool minus_lhs = (lhs.size() && lhs.front() == '-');
					std::string unsigned_lhs = minus_lhs ? lhs.substr(1) : lhs;
					bool minus_rhs = (rhs.size() && rhs.front() == '-');
					std::string unsigned_rhs = minus_rhs ? rhs.substr(1) : rhs;
					if(minus_lhs){
						if(minus_rhs){
							return "-" + string_unsigned_integer_add(unsigned_lhs, unsigned_rhs);
						}else{
							return string_unsigned_integer_minus(unsigned_rhs, unsigned_lhs);
						}
					}else{
						if(minus_rhs){
							return string_unsigned_integer_minus(unsigned_lhs, unsigned_rhs);
						} else{
							return string_unsigned_integer_add(unsigned_lhs, unsigned_rhs);
						}
					}
				}
				static std::string string_signed_integer_minus(const std::string& lhs, const std::string& rhs)
				{
					if(lhs.empty() || rhs.empty()) return std::string();
					if(rhs == "0") return lhs;
					bool minus_rhs = (rhs.size() && rhs.front() == '-');
					return string_signed_integer_add(lhs, minus_rhs ? rhs.substr(1) : ("-" + rhs));
				}
				static std::string normalize_number(const std::string& number)
				{
					std::string integer;
					std::string fractional;
					std::string exponent;
					bool minus = false;
					auto it = number.begin();
					auto end = number.end();
					if(it != end && *it == '-'){
						minus = true;
						++it;
					}
					if(it == end) return std::string();
					if(*it == '0'){
						integer = "0";
						++it;
					}else if('1' <= *it && *it <= '9'){
						while(it != end && '0' <= *it && *it <= '9'){
							integer += *it;
							++it;
						}
					}else{
						return std::string();
					}
					if(it != end && *it == '.'){
						++it;
						while(it != end && '0' <= *it && *it <= '9'){
							fractional += *it;
							++it;
						}
						if(fractional.empty()){
							return std::string();
						}
					}
					if(it != end && (*it == 'e' || *it == 'E')){
						++it;
						bool exponent_minus = false;
						if(it != end && *it == '-'){
							exponent_minus = true;
							++it;
						} else if(it != end && *it == '+'){
							++it;
						}
						while(it != end && '0' <= *it && *it <= '9'){
							exponent += *it;
							++it;
						}
						if(!exponent.empty()){
							return std::string();
						}
						while(exponent.size() > 1 && exponent.front() == '0'){
							exponent = exponent.substr(1);
						}
						if(exponent_minus && exponent != "0") exponent = "-" + exponent;
					}else{
						exponent = "0";
					}
					if(it != end){
						return std::string();
					}
					while(!fractional.empty() && fractional.back() == '0'){
						fractional.pop_back();
					}
					while(integer.size() > 1 && integer.front() == '0'){
						integer = integer.substr(1);
					}
					// 0 or (-)[1-9].[0-9]*e(-)[0-9]*
					// 0.123 -> 1.23e-1
					while(integer == "0" && !fractional.empty()){
						integer = fractional.substr(0, 1);
						fractional = fractional.substr(1);
						exponent = string_signed_integer_minus(exponent, "1");
					}
					// 12.3 -> 1.23e1
					while(integer.size() > 1){
						fractional = integer.substr(integer.size() - 1) + fractional;
						integer = integer.substr(0, integer.size() - 1);
						exponent = string_signed_integer_add(exponent, "1");
					}
					if(integer == "0" && fractional.empty()){
						//just 0
						exponent = "0";
					}else{
						if(minus){
							integer = "-" + integer;
						}
					}
					std::string result = integer;
					if(!fractional.empty()){
						result += "." + fractional;
					}
					if(exponent != "0"){
						result += "e" + exponent;
					}
					return result;
				}
				static bool test(const json_value& lhs, const json_value& rhs)
				{
					if(!lhs.is_valid_type()) return false;
					if(!rhs.is_valid_type()) return false;
					if(lhs.get_type() != rhs.get_type()){
						return false;
					}
					switch(lhs.get_type()){
					case number_type:
						if(normalize_number(lhs.get_number()) != normalize_number(rhs.get_number())){
							return false;
						}
						return true;
					case string_type:
						if(lhs.get_string() != rhs.get_string()){
							return false;
						}
						return true;
					case array_type:
					{
						if(lhs.size() != rhs.size()){
							return false;
						}
						for(auto it = lhs.begin_array(), end = lhs.end_array(), rit = rhs.begin_array(); it != end; ++it, ++rit){
							if(!*it || !*rit){
								return false;
							}
							if(!test(**it, **rit)){
								return false;
							}
						}
						break;
					}
					case object_type:
					{
						if(lhs.size() != rhs.size()){
							return false;
						}
						for(auto it = lhs.begin_key(), end = lhs.end_key(); it != end; ++it){
							if(!*it || !(*it)->is_string()){
								return false;
							}
							auto key = (*it)->get_string();
							auto lhs_value = lhs.get_member(key);
							if(!lhs_value){
								return false;
							}
							auto rhs_value = rhs.get_member(key);
							if(!rhs_value){
								return false;
							}
							if(!test(*lhs_value, *rhs_value)){
								return false;
							}
						}
						break;
					}
					}
					return true;
				}
			};
			static std::shared_ptr<json_value> json_patch(std::shared_ptr<json_value> target, const std::shared_ptr<json_value>& patch)
			{
				if(!patch || !target || !patch->is_array()){
					return nullptr;
				}
				target = target->clone();
				for(auto it = patch->begin_array(), end = patch->end_array(); it != end; ++it){
					auto member = *it;
					if(!member || !member->is_object()){
						return false;
					}
					auto op_ptr = member->get_member("op");
					if(!op_ptr || !op_ptr->is_string()){
						return false;
					}
					auto op = op_ptr->get_string();
					auto path_ptr = member->get_member("path");
					if(!path_ptr || !path_ptr->is_string()){
						return false;
					}
					auto path = path_ptr->get_string();
					if(op == "add"){
						auto value_ptr = member->get_member("value");
						if(!value_ptr){
							return false;
						}
						json_patch_add jp(value_ptr);
						if(!jp.parse(path)){
							return false;
						}
						if(!jp.evaluate(target)){
							return false;
						}
					}else if(op == "remove"){
						json_patch_remove jp;
						if(!jp.parse(path)){
							return false;
						}
						if(!jp.evaluate(target)){
							return false;
						}
					}else if(op == "replace"){
						auto value_ptr = member->get_member("value");
						if(!value_ptr){
							return false;
						}
						json_patch_replace jp(value_ptr);
						if(!jp.parse(path)){
							return false;
						}
						if(!jp.evaluate(target)){
							return false;
						}
					}else if(op == "move"){
						auto from_ptr = member->get_member("from");
						if(!from_ptr || !from_ptr->is_string()){
							return false;
						}
						auto from = from_ptr->get_string();
						json_patch_remove jp_from;
						if(!jp_from.parse(from)){
							return false;
						}
						if(!jp_from.evaluate(target)){
							return false;
						}
						if(!jp_from.get()){
							return false;
						}
						json_patch_add jp(jp_from.get());
						if(!jp.parse(path)){
							return false;
						}
						if(!jp.evaluate(target)){
							return false;
						}
					}else if(op == "copy"){
						auto from_ptr = member->get_member("from");
						if(!from_ptr || !from_ptr->is_string()){
							return false;
						}
						auto from = from_ptr->get_string();
						json_patch_get jp_from;
						if(!jp_from.parse(from)){
							return false;
						}
						if(!jp_from.evaluate(target)){
							return false;
						}
						if(!jp_from.get()){
							return false;
						}
						json_patch_add jp(jp_from.get()->clone());
						if(!jp.parse(path)){
							return false;
						}
						if(!jp.evaluate(target)){
							return false;
						}
					}else if(op == "test"){
						auto value_ptr = member->get_member("value");
						if(!value_ptr){
							return false;
						}
						json_patch_test jp(value_ptr);
						if(!jp.parse(path)){
							return false;
						}
						if(!jp.evaluate(target)){
							return false;
						}
					}else{
						return false;
					}
				}
				return target;
			}
		};
	}
}