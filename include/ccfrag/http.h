#pragma once
#include <string>
#include <deque>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <memory>
#include <cctype>
#include <algorithm>
#include <sstream>

namespace ccfrag{
	// specification
	// https://tools.ietf.org/html/rfc3986
	class url
	{
	public:
		typedef bool(*char_check_function)(char);
		class input_type
		{
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
			bool empty() const { return end <= begin; }
			size_t size() const { return end - begin; }
			char front() const { return !empty() ? *begin : '\0'; }
			char operator[](size_t index) const { return begin[index]; }
			char at(size_t index) const { return index <= size() ? begin[index] : '\0'; }
			void advance(size_t count) { begin += count; }
			size_t find(char c, size_t offset = 0)
			{
				for(auto it = begin + offset; it < end; ++it){
					if(*it == c){
						return it - begin;
					}
				}
				return std::string::npos;
			}
			size_t find(const std::string& str, size_t offset = 0)
			{
				if(str.empty() || size() < str.size()){
					return std::string::npos;
				}
				for(auto it = begin + offset; it < end - (str.size() - 1); ++it){
					if(std::equal(it, it + str.size(), str.begin())){
						return it - begin;
					}
				}
				return std::string::npos;
			}
			bool is_front(char c)
			{
				if(!empty() &&
					front() == c){
					++begin;
					return true;
				}
				return false;
			}
			bool is_front(char_check_function func)
			{
				if(!empty() &&
					func(front())){
					++begin;
					return true;
				}
				return false;
			}
			bool is_front(const std::string& str)
			{
				input_type wk = *this;
				for(auto it = str.begin(); it != str.end(); ++it){
					if(!is_front(*it)){
						*this = wk;
						return false;
					}
				}
				return true;
			}
			size_t operator-(const input_type& rhs) const
			{
				return begin - rhs.begin;
			}
			input_type& operator+=(size_t rhs)
			{
				advance(rhs);
				return *this;
			}
			std::string diff(const input_type& rhs) const
			{
				return std::string(begin, rhs.begin);
			}
			std::string substr(size_t offset = 0, size_t count = std::string::npos) const
			{
				if(count == std::string::npos){
					return std::string(begin + offset);
				}
				return std::string(begin + offset, begin + offset + count);
			}
			bool is_start(char_check_function func, size_t minimum)
			{
				input_type wk = *this;
				while(!wk.is_front(func)){
					if(!func(wk.front())){
						break;
					}
					++wk.begin;
				}
				size_t count = wk - *this;
				if(count < minimum){
					return false;
				}
				*this = wk;
				return true;
			}
			bool parse_pct_encoded(char& decoded_char)
			{
				if(size() <= 2){
					return false;
				}
				if(front() == '%' &&
					is_hex(begin[1]) &&
					is_hex(begin[2])){
					decoded_char = static_cast<char>(to_hex(begin[1]) * 16 + to_hex(begin[2]));
					*this += 3;
					return true;
				}
				return false;
			}
			bool parse_string(std::string& string, char_check_function func, bool allow_pct_encoded)
			{
				if(empty()){
					return true;
				}
				while(!empty()){
					char c = front();
					if(func(c)){
						string.push_back(c);
						advance(1);
						continue;
					}
					if(allow_pct_encoded){
						if(parse_pct_encoded(c)){
							if(func(c)){
								string.push_back(c);
								continue;
							}
							string += pct_encode(c);
							continue;
						}
					}
					break;
				}
				return true;
			}
		};
		static char tolower(char c)
		{
			if('A' <= c && c <= 'Z'){
				return c - 'A' + 'a';
			}
			return c;
		}
		class scheme_type{
		public:
			std::string scheme;
			scheme_type()
			{
			}
			scheme_type(const scheme_type& rhs)
			: scheme(rhs.scheme)
			{
			}
			void clear()
			{
				scheme.clear();
			}
			std::string get() const
			{
				return scheme;
			}
			static bool is_scheme(char c)
			{
				static const std::string scheme_chars = "+-.";
				return is_alpha(c) || is_digit(c) || scheme_chars.find(c) != std::string::npos;
			}
			bool parse(input_type& in)
			{
				input_type tmp = in;
				if(!tmp.is_front(is_alpha)){
					return false;
				}
				while(tmp.is_front(is_scheme)){
					;
				}
				scheme = in.diff(tmp);
				in = tmp;
				return true;
			}
			bool operator==(const scheme_type& rhs) const
			{
				return scheme == rhs.scheme;
			}
			std::shared_ptr<scheme_type> clone() const
			{
				return std::make_shared<scheme_type>(*this);
			}
			std::string normalize()
			{
				std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](char c) { return tolower(c); });
			}
		};
		struct authority_type
		{
			std::string userinfo;
			std::string host;
			std::string port;
			bool literal; ///< host in []
			authority_type()
			: literal(false)
			{
			}
			authority_type(const authority_type& rhs)
				: userinfo(rhs.userinfo)
				, host(rhs.host)
				, port(rhs.port)
				, literal(rhs.literal)
			{
			}
			void clear()
			{
				userinfo.clear();
				host.clear();
				port.clear();
				literal = false;
			}
			std::string get() const
			{
				std::string result;
				if(!userinfo.empty()){
					result = escape_userinfo(userinfo) + "@";
				}
				if(literal){
					result += "[" + host + "]";
				}else{
					result += escape_regname(host);
				}
				if(!port.empty()){
					result = ":" + port;
				}
				return result;
			}
			static bool is_userinfo(char c)
			{
				return c == ':' || is_unreserved(c) || is_sub_delims(c);
			}
			bool parse_userinfo(input_type& in)
			{
				return in.parse_string(userinfo, is_userinfo, true);
			}
			static std::string escape_userinfo(const std::string& input)
			{
				return escape(input, is_userinfo);
			}
			bool parse_host(input_type& in)
			{
				return parse_ip_literal(in) ||
					parse_ipv4address(in) ||
					parse_regname(in);
			}
			bool parse_ip_literal(input_type& in)
			{
				auto wk = in;
				if(!wk.is_front('[')){
					return false;
				}
				auto bracket_end = wk.find(']');
				if(bracket_end == std::string::npos){
					return false;
				}
				std::string content = wk.substr(0, bracket_end);
				if(parse_ipv_future(wk)){
				} else if(is_ipv6address(content)){
					host = content;
					literal = true;
					wk.advance(content.size());
				} else{
					return false;
				}
				if(!wk.is_front(']')){
					host.clear();
					literal = false;
					return false;
				}
				in = wk;
				return true;
			}
			static bool is_h16(const std::string& h16)
			{
				if(h16.size() < 1 || 4 < h16.size()){
					return false;
				}
				for(auto it = h16.begin(), end = h16.end(); it != end; ++it){
					if(!is_hex(*it)){
						return false;
					}
				}
				return true;
			}
			static bool is_ipv6address(const std::string& addr)
			{
				size_t double_colon = addr.find("::");
				if(double_colon == std::string::npos){
					std::string first_part = addr.substr(0, double_colon);
					std::string second_part = addr.substr(double_colon + 2);
					std::deque<std::string> first_h16;
					std::deque<std::string> second_h16;
					split(first_h16, first_part, ':');
					split(second_h16, second_part, ':');
					if(!second_h16.empty()){
						if(is_ipv4address(second_h16.back())){
							second_h16.pop_back();
							second_h16.push_back("0");//dummy h16
							second_h16.push_back("0");//dummy h16
						}
					}
					for(auto it = first_h16.begin(), end = first_h16.end(); it != end; ++it){
						if(!is_h16(*it)){
							return false;
						}
					}
					for(auto it = second_h16.begin(), end = second_h16.end(); it != end; ++it){
						if(!is_h16(*it)){
							return false;
						}
					}
					size_t h16_count = first_h16.size() + second_h16.size();
					if(8 <= h16_count){
						return false;
					}
					return true;
				}
				std::deque<std::string> h16;
				split(h16, addr, ':');
				if(!h16.empty()){
					if(is_ipv4address(h16.back())){
						h16.pop_back();
						h16.push_back("0");//dummy h16
						h16.push_back("0");//dummy h16
					}
				}
				if(h16.size() != 8){
					return false;
				}
				for(auto it = h16.begin(), end = h16.end(); it != end; ++it){
					if(!is_h16(*it)){
						return false;
					}
				}
				return true;
			}
			static bool is_ipv_future(char c)
			{
				return is_unreserved(c) || is_sub_delims(c) || c == ':';
			}
			bool parse_ipv_future(input_type& in)
			{
				auto wk = in;
				if(!wk.is_front('v')){
					return false;
				}
				if(!wk.is_start(is_hex, 1)){
					return false;
				}
				if(!wk.is_front('.')){
					return false;
				}
				if(!wk.is_start(is_ipv_future, 1)){
					return false;
				}
				host = in.diff(wk);
				literal = true;
				in = wk;
				return true;
			}
			static bool is_digit04(char c)
			{
				return ('0' <= c && c <= '4');
			}
			static bool is_digit05(char c)
			{
				return ('0' <= c && c <= '5');
			}
			static bool is_digit19(char c)
			{
				return ('1' <= c && c <= '9');
			}
			bool parse_dec_octet(input_type& in)
			{
				auto wk = in;
				if(wk.is_front('2') && wk.is_front('5') && wk.is_front(is_digit05)){
					in = wk;
					return true;
				}
				wk = in;
				if(wk.is_front('2') && wk.is_front(is_digit04) && wk.is_front(is_digit)){
					in = wk;
					return true;
				}
				wk = in;
				if(wk.is_front('1') && wk.is_front(is_digit) && wk.is_front(is_digit)){
					in = wk;
					return true;
				}
				wk = in;
				if(wk.is_front(is_digit19) && wk.is_front(is_digit)){
					in = wk;
					return true;
				}
				wk = in;
				if(wk.is_front(is_digit)){
					in = wk;
					return true;
				}
				return false;
			}
			bool parse_ipv4address(input_type& in)
			{
				auto wk = in;
				if(parse_dec_octet(wk) && wk.is_front('.') &&
					parse_dec_octet(wk) && wk.is_front('.') &&
					parse_dec_octet(wk) && wk.is_front('.') &&
					parse_dec_octet(wk)){
					host = in.diff(wk);
					literal = false;
					in = wk;
					return true;
				}
				return false;
			}
			static bool is_dec_octet(const std::string& dec_octet)
			{
				if(dec_octet.size() == 3){
					if(dec_octet[0] == '2'){
						if(dec_octet[1] == '5' && is_digit05(dec_octet[2])){
							return true;
						} else if(is_digit04(dec_octet[1]) && is_digit(dec_octet[2])){
							return true;
						}
					} else if(dec_octet[0] == '1'){
						if(is_digit(dec_octet[1]) && is_digit(dec_octet[2])){
							return true;
						}
					}
				} else if(dec_octet.size() == 2){
					if(is_digit19(dec_octet[0]) && is_digit(dec_octet[1])){
						return true;
					}
				} else if(dec_octet.size() == 1){
					if(is_digit(dec_octet[1])){
						return true;
					}
				}
				return false;
			}
			static bool is_ipv4address(const std::string& addr)
			{
				std::deque<std::string> dec_octets;
				split(dec_octets, addr, '.');
				if(dec_octets.size() != 4){
					return false;
				}
				for(auto it = dec_octets.begin(), end = dec_octets.end(); it != end; ++it){
					if(!is_dec_octet(*it)){
						return false;
					}
				}
				return true;
			}
			static bool is_regname(char c)
			{
				return is_unreserved(c) || is_sub_delims(c);
			}
			bool parse_regname(input_type& in)
			{
				if(in.parse_string(host, is_regname, true)){
					literal = false;
					return true;
				}
				return false;
			}
			static std::string escape_regname(const std::string& input)
			{
				return escape(input, is_regname);
			}
			bool parse_port(input_type& in)
			{
				return in.parse_string(port, is_digit, false);
			}
			bool parse(input_type& in)
			{
				auto wk = in;
				if(!parse_userinfo(wk) || !wk.is_front('@')){
					userinfo.clear();
					wk = in;
				}
				if(!parse_host(wk)){
					return false;
				}
				if(wk.is_front(':')){
					if(!parse_port(wk)){
						return false;
					}
				}
				in = wk;
				return true;
			}
			std::shared_ptr<authority_type> clone() const
			{
				return std::make_shared<authority_type>(*this);
			}
			std::string normalize()
			{
				std::transform(host.begin(), host.end(), host.begin(), [](char c) { return tolower(c); });
			}
		};
		struct path_type{
			std::deque<std::string> segments;
			enum path_types{
				path_type_unknown,
				path_type_abempty,   //< *("/" segment)
				path_type_absolute,  //< "/" [segment-nz *("/" segment)]
				path_type_noscheme,  //< segment-nz-nc *("/" segment)
				path_type_rootless,  //< segment-nz *("/" segment)
				path_type_empty,     //< 0<pchar>
			} type;
			enum parse_type{
				parse_none_flag = 0,
				parse_start_slash = 1,
				parse_nz = 2, ///< no zero
				parse_nc = 4, ///< no colon
				parse_nz_nc = parse_nz | parse_nc
			};
			path_type()
				: type(path_type_unknown)
			{
			}
			path_type(const path_type& rhs)
			: segments(rhs.segments)
			, type(rhs.type)
			{
			}
			void clear()
			{
				segments.clear();
				type = path_type_unknown;
			}
			std::string get() const
			{
				std::string path;
				for(auto it = segments.begin(), end = segments.end(); it != end; ++it){
					path += *it;
				}
				return path;
			}
			bool parse_path_abempty(input_type& in)
			{
				clear();
				while(parse_slash_segment(in, parse_start_slash)){
					;
				}
				type = path_type_abempty;
				return true;
			}
			bool parse_path_absolute(input_type& in)
			{
				auto wk = in;
				clear();
				if(!wk.is_front('/')){
					return false;
				}
				if(!parse_slash_segment(wk, parse_nz)){
					//no segment, but saved slash
					clear();
					segments.push_back("/");
					type = path_type_absolute;
					in = wk;
					return true;
				}
				if(segments.size() != 1){
					return false;
				}
				segments.front() = "/" + segments.front();
				while(parse_slash_segment(wk, parse_start_slash)){
					;
				}
				type = path_type_absolute;
				in = wk;
				return true;
			}
			bool parse_path_noscheme(input_type& in)
			{
				auto wk = in;
				clear();
				if(!parse_slash_segment(wk, parse_nz_nc)){
					return false;
				}
				while(parse_slash_segment(wk, parse_start_slash)){
					;
				}
				type = path_type_noscheme;
				in = wk;
				return true;
			}
			bool parse_path_rootless(input_type& in)
			{
				auto wk = in;
				clear();
				if(!parse_slash_segment(wk, parse_nz)){
					return false;
				}
				while(parse_slash_segment(wk, parse_start_slash)){
					;
				}
				type = path_type_rootless;
				in = wk;
				return true;
			}
			bool parse_path_empty(input_type&)
			{
				clear();
				type = path_type_empty;
				return true;
			}
			static bool is_pchar_nc(char c)
			{
				return is_unreserved(c) || is_sub_delims(c) || c == '@';
			}
			static bool is_pchar(char c)
			{
				return is_unreserved(c) || is_sub_delims(c) || c == ':' || c == '@';
			}
			bool parse_slash_segment(input_type& in, int parse_type)
			{
				const bool start_slash = (parse_type & parse_start_slash);
				const bool nz = (parse_type & parse_nz);
				const bool nc = (parse_type & parse_nc);
				auto wk = in;
				std::string segment;
				if(start_slash){
					if(!wk.is_front('/')){
						return false;
					}
					segment.push_back('/');
				}
				if(!wk.parse_string(segment, nc ? is_pchar_nc : is_pchar, true)){
					return false;
				}
				if(nz){
					if(segment.size() <= (start_slash ? 1 : 0)){
						return false;
					}
				}
				in = wk;
				segments.push_back(segment);
				return true;
			}
			static std::deque<std::string> remove_dot_segments(std::deque<std::string> input)
			{
				std::deque<std::string> output;
				while(!input.empty()){
					auto& segment = input.front();
					//2. A
					auto next = input.begin() + 1;
					if((segment == ".." || segment == ".") && next != input.end() && !next->empty() && next->front() == '/'){
						input.pop_front();
						next->erase(0, 1);
						continue;
					}
					//2. B
					if(segment == "/."){
						if(next != input.end() && !next->empty() && next->front() == '/'){
							input.pop_front();
							continue;
						}
						segment = "/";
						continue;
					}
					//2. C
					if(segment == "/.."){
						if(next != input.end() && !next->empty() && next->front() == '/'){
							input.pop_front();
							if(!output.empty()){
								output.pop_back();
							}
							continue;
						}
						segment = "/";
						if(!output.empty()){
							output.pop_back();
						}
						continue;
					}
					//2. D
					if(input.size() == 1 && (segment == "." || segment == "..")){
						input.clear();
						continue;
					}
					//2. E
					output.push_back(segment);
					input.pop_front();
				}
				return output;
			}
			static std::shared_ptr<path_type> merge(const path_type& base, const path_type& relative, bool base_authority_defined)
			{
				std::shared_ptr<path_type> result = std::make_shared<path_type>();
				if(base_authority_defined && base.get().empty()){
					if(relative.get().substr(0, 1) == "/"){
						result->segments = relative.segments;
					}else{
						result->segments = relative.segments;
						if(!result->segments.empty()){
							result->segments.front() = "/" + result->segments.front();
						}else{
							result->segments.push_back("/");
						}
					}
				}else{
					result->segments = base.segments;
					bool insert_slash = false;
					if(!result->segments.empty()){
						auto& last_segment = result->segments.back();
						if(!last_segment.empty() && last_segment.front() == '/'){
							insert_slash = true;
						}
						result->segments.pop_back();
					}
					for(auto it = relative.segments.begin(), end = relative.segments.end(); it != end; ++it){
						if(insert_slash){
							insert_slash = false;
							result->segments.push_back("/" + *it);
						}else{
							result->segments.push_back(*it);
						}
					}
				}
				return result;
			}
			std::shared_ptr<path_type> clone() const
			{
				return std::make_shared<path_type>(*this);
			}
			std::shared_ptr<path_type> remove_dot_segments() const
			{
				std::shared_ptr<path_type> result = std::make_shared<path_type>(*this);
				result->segments = path_type::remove_dot_segments(segments);
				result->type = path_type_unknown;
				return result;
			}
			bool empty() const
			{
				for(auto it = segments.begin(), end = segments.end(); it != end; ++it){
					if(!it->empty()){
						return false;
					}
				}
				return true;
			}
		};
		struct query_type{
			struct parameter_type{
				std::string key;
				bool has_equal;
				std::string value;
				parameter_type()
				: has_equal(false)
				{
				}
				void clear()
				{
					key.clear();
					has_equal = false;
					value.clear();
				}
				bool empty() const
				{
					return key.empty() && !has_equal && value.empty();
				}
				static bool is_key(char c)
				{
					if(c == '=') return false;
					return is_query(c);
				}
				static bool is_value(char c)
				{
					if(c == '&') return false;
					return is_query(c);
				}
				std::string get() const
				{
					std::string parameter;
					parameter = escape(key, is_key);
					if(has_equal){
						parameter += "=";
						parameter += escape(value, is_value);
					}
					return parameter;
				}
			};
			std::string value;
			std::list<parameter_type> parameters;
			query_type()
			{
			}
			query_type(const query_type& rhs)
			: value(rhs.value)
			, parameters(rhs.parameters)
			{
			}
			void clear()
			{
				value.clear();
				parameters.clear();
			}
			static bool is_query(char c)
			{
				return path_type::is_pchar(c) || c == '/' || c == '?';
			}
			bool parse(input_type& in)
			{
				auto wk = in;
				bool is_key = true;
				parameter_type parameter;
				while(!wk.empty()){
					char c = wk.front();
					bool encoded = wk.parse_pct_encoded(c);
					if(encoded || wk.is_front(is_query)){
						if(!parameter.has_equal && !encoded && c == '='){
							parameter.has_equal = true;
						}else if(!encoded && c == '&'){
							parameters.push_back(parameter);
							parameter.clear();
						}else if(!parameter.has_equal){
							parameter.key.push_back(c);
						}else{
							parameter.value.push_back(c);
						}
						continue;
					}
					break;
				}
				if(!parameter.empty()){
					parameters.push_back(parameter);
				}
				value = in.diff(wk);
				in = wk;
				return true;
			}
			std::string get() const
			{
				std::string query;
				for(auto it = parameters.begin(), begin = parameters.begin(), end = parameters.end(); it != end; ++it){
					if(it != begin){
						query += "&";
					}
					query += it->get();
				}
				return query;
			}
			std::shared_ptr<query_type> clone() const
			{
				return std::make_shared<query_type>(*this);
			}
		};
		struct fragment_type{
			std::string value; //< escaped
			std::string fragment; //< decoded
			fragment_type()
			{
			}
			fragment_type(const fragment_type& rhs)
				: value(rhs.value)
				, fragment(rhs.fragment)
			{
			}
			void clear()
			{
				value.clear();
				fragment.clear();
			}
			static bool is_fragment(char c)
			{
				return path_type::is_pchar(c) || c == '/' || c == '?';
			}
			bool parse(input_type& in)
			{
				auto wk = in;
				bool is_key = true;
				while(!wk.empty()){
					char c = wk.front();
					bool encoded = wk.parse_pct_encoded(c);
					if(encoded || wk.is_front(is_fragment)){
						fragment.push_back(c);
						continue;
					}
					break;
				}
				value = in.diff(wk);
				in = wk;
				return true;
			}
			std::string get() const
			{
				return escape(fragment, is_fragment);
			}
			std::shared_ptr<fragment_type> clone() const
			{
				return std::make_shared<fragment_type>(*this);
			}
		};
		std::shared_ptr<scheme_type> scheme;
		std::shared_ptr<authority_type> authority;
		std::shared_ptr<path_type> path;
		std::shared_ptr<query_type> query;
		std::shared_ptr<fragment_type> fragment;
		url()
		{
		}
		url(const url& rhs)
		{
			if(rhs.scheme) scheme = rhs.scheme->clone();
			if(rhs.authority) authority = rhs.authority->clone();
			if(rhs.path) path = rhs.path->clone();
			if(rhs.query) query = rhs.query->clone();
			if(rhs.fragment) fragment = rhs.fragment->clone();
		}
		url(const std::string& url_string)
		{
			if(!parse_uri(url_string)){
				clear();
			}
		}
		std::shared_ptr<url> clone() const
		{
			return std::make_shared<url>(*this);
		}
		void clear()
		{
			scheme.reset();
			authority.reset();
			path.reset();
			query.reset();
			fragment.reset();
		}
		static void split(std::deque<std::string>& result, const std::string& str, char delimiter)
		{
			size_t offset = 0;
			while(offset < str.size()){
				size_t end = str.find(delimiter, offset);
				if(end == std::string::npos){
					result.push_back(str.substr(offset));
					return;
				}
				result.push_back(str.substr(offset, end));
				offset = end + 1;
			}
		}
		static bool is_reserved(char c)
		{
			return is_gen_delims(c) || is_sub_delims(c);
		}
		static bool is_gen_delims(char c)
		{
			static const std::string delims = ":/?#[]@";
			return delims.find(c) != std::string::npos;
		}
		static bool is_sub_delims(char c)
		{
			static const std::string delims = "!$&'()*+,;=";
			return delims.find(c) != std::string::npos;
		}
		static bool is_alpha(char c)
		{
			return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
		}
		static bool is_digit(char c)
		{
			return ('0' <= c && c <= '9');
		}
		static bool is_hex(char c)
		{
			return is_digit(c) || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');
		}
		static int to_hex(char c)
		{
			if(is_digit(c)){
				return c - '0';
			}
			if(('a' <= c && c <= 'f')){
				return c - 'a' + 10;
			}
			if(('A' <= c && c <= 'F')){
				return c - 'A' + 10;
			}
			return -1;
		}
		static char from_hex(char hex)
		{
			if(hex < 10){
				return static_cast<char>(hex + '0');
			}
			if(hex < 16){
				return static_cast<char>(hex - 10 + 'A'); //for case normalization
			}
			return '0';
		}
		static bool is_unreserved(char c)
		{
			static const std::string unreserved = "-._~";
			return is_alpha(c) || is_digit(c) || unreserved.find(c) != std::string::npos;
		}
		static std::string pct_encode(char decoded_char)
		{
			std::string result;
			result.reserve(3);
			result.push_back('%');
			result.push_back(from_hex((decoded_char >> 4) & 0xf));
			result.push_back(from_hex(decoded_char & 0xf));
			return result;
		}
		static std::string escape(const std::string& input, char_check_function func)
		{
			std::string result;
			for(auto it = input.begin(), end = input.end(); it != end; ++it){
				auto c = *it;
				if(func(c)){
					result.push_back(c);
					continue;
				}
				result += pct_encode(c);
			}
			return result;
		}
		bool parse_uri(const std::string& uri)
		{
			input_type in(uri.c_str(), uri.c_str() + uri.size());
			scheme = std::make_shared<scheme_type>();
			if(!scheme->parse(in)){
				clear();
				return false;
			}
			if(!in.is_front(':')){
				return false;
			}
			if(!parse_hier_part(in)){
				return false;
			}
			if(in.is_front('?')){
				query = std::make_shared<query_type>();
				if(!query->parse(in)){
					return false;
				}
			}
			if(in.is_front('#')){
				fragment = std::make_shared<fragment_type>();
				if(!fragment->parse(in)){
					return false;
				}
			}
			if(!in.empty()){
				return false;
			}
			return true;
		}
		bool parse_uri_reference(const std::string& uri)
		{
			if(parse_uri(uri)){
				return true;
			}
			clear();
			input_type in(uri.c_str(), uri.c_str() + uri.size());
			if(!parse_relative_part(in)){
				return false;
			}
			if(in.is_front('?')){
				query = std::make_shared<query_type>();
				if(!query->parse(in)){
					return false;
				}
			}
			if(in.is_front('#')){
				fragment = std::make_shared<fragment_type>();
				if(!fragment->parse(in)){
					return false;
				}
			}
			if(!in.empty()){
				return false;
			}
			return true;
		}
		bool parse_hier_part(input_type& in, bool relative = false)
		{
			auto wk = in;
			if(wk.substr(0, 2) == "//"){
				wk.advance(2);
				authority = std::make_shared<authority_type>();
				path = std::make_shared<path_type>();
				if(authority->parse(wk) &&
					path->parse_path_abempty(wk)){
					in = wk;
					return true;
				}
				authority.reset();
			}
			path = std::make_shared<path_type>();
			if(path->parse_path_absolute(in)) return true;
			if(!relative){
				if(path->parse_path_rootless(in)) return true;
			}else{
				if(path->parse_path_noscheme(in)) return true;
			}
			if(path->parse_path_empty(in)) return true;
			path.reset();
			return false;
		}
		bool parse_relative_part(input_type& in)
		{
			return parse_hier_part(in, true);
		}
		bool parse_origin_form(const std::string& str)
		{
			clear();
			input_type in(str.c_str(), str.c_str() + str.size());
			path = std::make_shared<path_type>();
			if(!path->parse_path_absolute(in)) {
				return false;
			}
			if(in.is_front('?')){
				query = std::make_shared<query_type>();
				if(!query->parse(in)){
					return false;
				}
			}
			if(!in.empty()){
				return false;
			}
			return true;
		}
		bool parse_absolute_uri(const std::string& uri)
		{
			clear();
			input_type in(uri.c_str(), uri.c_str() + uri.size());
			scheme = std::make_shared<scheme_type>();
			if(!scheme->parse(in)){
				clear();
				return false;
			}
			if(!in.is_front(':')){
				return false;
			}
			if(!parse_hier_part(in)){
				return false;
			}
			if(in.is_front('?')){
				query = std::make_shared<query_type>();
				if(!query->parse(in)){
					return false;
				}
			}
			if(!in.empty()){
				return false;
			}
			return true;
		}
		bool parse_authority(const std::string& str)
		{
			clear();
			input_type in(str.c_str(), str.c_str() + str.size());
			if(!authority->parse(in)){
				return false;
			}
			if(!in.empty()){
				return false;
			}
			return true;
		}
		static std::shared_ptr<url> resolve(const url& base, const url& relative, bool strict = true)
		{
			url target;
			bool relative_scheme_undefined = (relative.scheme == nullptr);
			if(!strict && relative.scheme && base.scheme && *relative.scheme == *base.scheme){
				relative_scheme_undefined = true;
			}
			if(!relative_scheme_undefined && relative.scheme){
				target.scheme = relative.scheme;
				target.authority = relative.authority;
				target.path = relative.path->remove_dot_segments();
				target.query = relative.query;
			}else{
				if(relative.authority){
					target.authority = relative.authority;
					target.path = relative.path->remove_dot_segments();
					target.query = relative.query;
				}else{
					if(!relative.path) return nullptr;
					if(relative.path->empty()){
						target.path = base.path;
						if(relative.query){
							target.query = relative.query;
						}else{
							target.query = base.query;
						}
					}else{
						if(relative.path->get().substr(0, 1) == "/"){
							target.path = relative.path->remove_dot_segments();
						}else{
							if(!base.path) return nullptr;
							target.path = path_type::merge(*base.path, *relative.path, base.authority != nullptr);
							target.path = target.path->remove_dot_segments();
						}
						target.query = relative.query;
					}
					target.authority = base.authority;
				}
				target.scheme = base.scheme;
			}
			target.fragment = relative.fragment;
			return target.clone();
		}
		std::string get() const
		{
			std::string result;
			if(scheme){
				result += scheme->get() + ":";
			}
			if(authority){
				result += "//" + authority->get();
			}
			if(path){
				result += path->get();
			}
			if(query){
				result += "?" + query->get();
			}
			if(fragment){
				result += "#" + fragment->get();
			}
			return result;
		}
	};
	// https://tools.ietf.org/html/rfc7230
	class http
	{
	public:
		typedef bool(*char_check_function)(char);
		class input_type : public url::input_type {
		public:
			typedef url::input_type parent_type;
			input_type(const input_type& rhs)
				: parent_type(rhs)
			{
			}
			input_type(const char* begin, const char* end)
				: parent_type(begin, end)
			{
			}
			bool parse_token(std::string& string, char_check_function func, size_t minimum)
			{
				if(empty()){
					return false;
				}
				auto backup = *this;
				while(!empty()){
					char c = front();
					if(func(c)){
						string.push_back(c);
						advance(1);
						continue;
					}
					break;
				}
				if(minimum <= string.size()){
					return true;
				}
				*this = backup;
				return false;
			}
			bool parse_field_content(std::string& string)
			{
				bool ws = false;
				while(!empty()){
					char c = front();
					if(is_ws(c)){
						if(ws){
							continue;
						}
						ws = true;
						string.push_back(c);
						advance(1);
						continue;
					}
					ws = false;
					if(is_field_vchar(c)){
						string.push_back(c);
						advance(1);
						continue;
					}
					break;
				}
				return true;
			}
		};
		// https://tools.ietf.org/html/rfc7231
		class status_codes {
		public:
			enum {
				UNDETERMINED = -1,
				CONTINUE = 100,
				SWITCHING_PROTOCOLS = 101,
				OK = 200,
				CREATED = 201,
				ACCEPTED = 202,
				NON_AUTHORITATIVE_INFORMATION = 203,
				NO_CONTENT = 204,
				RESET_CONTENT = 205,
				PARTIAL_CONTENT = 206,
				MULTIPLE_CHOICES = 300,
				MOVED_PERMANENTLY = 301,
				FOUND = 302,
				SEE_OTHER = 303,
				NOT_MODIFIED = 304,
				USE_PROXY = 305,
				TEMPORARY_REDIRECT = 307,
				BAD_REQUEST = 400,
				UNAUTHORIZED = 401,
				PAYMENT_REQUIRED = 402,
				FORBIDDEN = 403,
				NOT_FOUND = 404,
				METHOD_NOT_ALLOWED = 405,
				NOT_ACCEPTABLE = 406,
				PROXY_AUTHENTICATION_REQUIRED = 407,
				REQUEST_TIMEOUT = 408,
				CONFLICT = 409,
				GONE = 410,
				LENGTH_REQUIRED = 411,
				PRECONDITION_FAILED = 412,
				PAYLOAD_TOO_LARGE = 413,
				URI_TOO_LONG = 414,
				UNSUPPORTED_MEDIA_TYPE = 415,
				RANGE_NOT_SATISFIABLE = 416,
				EXPECTATION_FAILED = 417,
				UPGRADE_REQUIRED = 426,
				INTERNAL_SERVER_ERROR = 500,
				NOT_IMPLEMENTED = 501,
				BAD_GATEWAY = 502,
				SERVICE_UNAVAILABLE = 503,
				GATEWAY_TIMEOUT = 504,
				HTTP_VERSION_NOT_SUPPORTED = 505,
			};
			static std::string to_str(int status_code)
			{
				return "UNKNOWN";
			}
			static bool is_infomational(int status_code){ return status_code / 100 == 1; }
			static bool is_successful(int status_code){ return status_code / 100 == 2; }
		};
		struct http_header
		{
			url request_target;
			char http_major_version;
			char http_minor_version;
			int status_code;
			std::string reason_phrase;
			//multiple header for comma separated list #(values) or Set-Cookie
			std::map<std::string, std::list<std::string> > headers;
			class transfer_encoding{
			public:
				class transfer_coding{
				public:
					enum codings{
						chunked,
						compress,
						deflate,
						gzip,
						unknown_coding,
					};
					codings coding;
					std::string extension;
					std::list<std::pair<std::string, std::string> > parameters;
					transfer_coding()
						: coding(unknown_coding)
					{
					}
				};
				std::list<transfer_coding> transfer_codings;
				bool parse(const std::string& value)
				{
					input_type in(value.c_str(), value.c_str() + value.size());
					do{
						while(in.is_front(is_ws)); // OWS
						transfer_coding tc;
						if(!parse_token(tc.extension, in)){
							return false;
						}
						tc.extension = to_lower(tc.extension);
						if(tc.extension == "chunked"){
							tc.coding = transfer_coding::chunked;
							transfer_codings.push_back(tc);
						} else if(tc.extension == "compress" || tc.extension == "x-compress"){
							tc.coding = transfer_coding::compress;
							transfer_codings.push_back(tc);
						} else if(tc.extension == "deflate"){
							tc.coding = transfer_coding::deflate;
							transfer_codings.push_back(tc);
						} else if(tc.extension == "gzip" || tc.extension == "x-gzip"){
							tc.coding = transfer_coding::gzip;
							transfer_codings.push_back(tc);
						} else{
							while(in.is_front(is_ws)); // OWS
							while(in.is_front(';')){
								while(in.is_front(is_ws)); // OWS
														   //transfer-parameter
								std::string parameter;
								if(!parse_token(parameter, in)){
									return false;
								}
								while(in.is_front(is_ws)); // BWS
								if(!in.is_front('=')){
									return false;
								}
								while(in.is_front(is_ws)); // BWS
								std::string value;
								if(!parse_token(value, in) &&
									!parse_quoted_string(value, in)){
									return false;
								}
								tc.parameters.push_back(std::make_pair(to_lower(parameter), value));
								while(in.is_front(is_ws)); // OWS
							}
						}
						while(in.is_front(is_ws)); // OWS
					} while(!in.empty() && in.is_front(','));
					return in.empty();
				}
				bool parse(const std::list<std::string>& values)
				{
					for(auto it = values.begin(), end = values.end(); it != end; ++it){
						if(!parse(*it)){
							return false;
						}
					}
					return true;
				}
				bool has_coding(transfer_coding::codings coding) const
				{
					for(auto it = transfer_codings.begin(), end = transfer_codings.end(); it != end; ++it){
						if(it->coding == coding) return true;
					}
					return false;
				}
				bool is_chunked() const { return has_coding(transfer_coding::chunked); }
				bool is_compress() const { return has_coding(transfer_coding::compress); }
				bool is_deflate() const { return has_coding(transfer_coding::deflate); }
				bool is_gzip() const { return has_coding(transfer_coding::gzip); }
				bool is_supported() const { return !has_coding(transfer_coding::unknown_coding); }
				bool chunked_is_only_last() const
				{
					if(!is_chunked()){
						return false;
					}
					if(transfer_codings.empty()){
						return false;
					}
					for(auto it = transfer_codings.rbegin(), end = transfer_codings.rend(); it != end; ++it){
						if(it == transfer_codings.rbegin()){
							if(it->coding != transfer_coding::chunked) return false;
						} else{
							if(it->coding == transfer_coding::chunked) return false;
						}
					}
					return true;
				}
				bool end_chunk(std::vector<char>& body)
				{
					if(!is_chunked()){
						return false;
					}
					if(transfer_codings.empty()){
						return false;
					}
					if(transfer_codings.back().coding != transfer_coding::chunked){
						return false;
					}
					auto it = transfer_codings.rbegin();
					++it;
					for(auto end = transfer_codings.rend(); it != end; ++it){
						switch(it->coding){
						case transfer_coding::compress:
							break;
						case transfer_coding::deflate:
							break;
						case transfer_coding::gzip:
							break;
						default:
							return false;
						}
					}
					return true;
				}
			} transfer_encoding;
			http_header()
			: http_major_version(0)
			, http_minor_version(0)
			, status_code(status_codes::UNDETERMINED)
			{
			}
			bool is_parsed() const
			{
				return http_major_version || http_minor_version;
			}
			bool is_http_1_0() const { return http_major_version == '1' && http_minor_version == '0'; }
			bool is_http_1_1() const { return http_major_version == '1' && http_minor_version == '1'; }
			static char tolower(char c)
			{
				if('A' <= c && c <= 'Z'){
					return c - 'A' + 'a';
				}
				return c;
			}
			static char toupper(char c)
			{
				if('a' <= c && c <= 'z'){
					return c - 'a' + 'A';
				}
				return c;
			}
			static std::string normalize_field_name(const std::string& field_name)
			{
				bool start = true;
				std::string result;
				result.reserve(field_name.size());
				for(auto it = field_name.begin(), end = field_name.end(); it != end; ++it){
					if(url::is_alpha(*it)){
						if(start){
							start = false;
							result.push_back(toupper(*it));
							continue;
						}
					}else{
						start = true;
					}
					result.push_back(*it);
				}
				return result;
			}
			bool has_header(const std::string& name) const
			{
				return headers.find(normalize_field_name(name)) != headers.end();
			}
			bool check_request_header(http_header& response_header)
			{
				//check transfer-encoding
				if(has_header("Transfer-Encoding")){
					if(!transfer_encoding.parse(headers["Transfer-Encoding"]) ||
						!transfer_encoding.chunked_is_only_last()){
						response_header.reason_phrase = "Invalid Transfer-Encoding";
						response_header.status_code = status_codes::BAD_REQUEST;
						return false;
					}
					if(!transfer_encoding.is_supported()){
						response_header.reason_phrase = "Not supported Transfer-Encoding";
						response_header.status_code = status_codes::NOT_IMPLEMENTED;
						return false;
					}
					if(has_header("Content-Length")){
						response_header.reason_phrase = "no set Transfer-Encoding and Content-Length both";
						response_header.status_code = status_codes::BAD_REQUEST;
						return false;
					}
				}
				return true;
			}
		};
		static input_type http_message_has_empty_line(std::string& http_message)
		{
			auto r = http_message.find("\r\n\r\n");
			if(r != std::string::npos){
				return input_type(http_message.c_str(), http_message.c_str() + r + 4);
			}
			return input_type(nullptr, nullptr);
		}
		static input_type get_line(input_type& in)
		{
			auto r = in.find("\r\n");
			if(r != std::string::npos){
				input_type line(in.begin, in.begin + r + 2);
				in.advance(r + 2);
				return line;
			}
			return input_type(nullptr, nullptr);
		}
		class http_message {
		public:
			http_header request_header;
			http_header response_header;
			std::vector<char> request_body;
			std::vector<char> response_body;
		};
		static bool parse_request(http_message& message, std::string& data)
		{
			static const size_t header_max_size = 8 * 1024;
			if(!message.request_header.is_parsed()){
				message.request_body.clear();
				auto in = http_message_has_empty_line(data);
				if(in.empty()){
					//recving data
					return false;
				}
				if(header_max_size < in.size()){
					message.response_header.reason_phrase = "Header is too big";
					message.response_header.status_code = status_codes::BAD_REQUEST;
					return false;
				}
				auto start_line = get_line(in);
				if(!parse_request_line(message.request_header, start_line)){
					message.response_header.reason_phrase = "Invalid header format";
					message.response_header.status_code = status_codes::BAD_REQUEST;
					return false;
				}
				if(!parse_header_fields(message.request_header, in)){
					message.response_header.reason_phrase = "Invalid header format";
					message.response_header.status_code = status_codes::BAD_REQUEST;
					return false;
				}
				size_t length = in.begin - data.c_str();
				data.erase(data.begin(), data.begin() + length);
				if(!message.request_header.check_request_header(message.response_header)){
					return false;
				}
			}
			if(message.request_header.transfer_encoding.is_chunked()){
				if(!parse_chunked_body(message, data)){
					return false;
				}
			}
			return true;
		}
		static bool parse_response_header(http_header& header, std::string& http_message)
		{
			static const size_t header_max_size = 8 * 1024;
			if(header_max_size < http_message.size()){
				return false;
			}
			auto in = http_message_has_empty_line(http_message);
			if(in.empty()){
				return false;
			}
			auto start_line = get_line(in);
			if(!parse_status_line(header, start_line)){
				return false;
			}
			if(!parse_header_fields(header, in)){
				return false;
			}
			size_t length = in.begin - http_message.c_str();
			http_message.erase(http_message.begin(), http_message.begin() + length);
			return true;
		}
		static bool parse_start_line(http_header& header, input_type& in)
		{
			return parse_request_line(header, in) ||
				parse_status_line(header, in);
		}
		static bool parse_request_line(http_header& header, input_type& in)
		{
			auto wk = in;
			std::string token;
			if(!parse_token(token, wk) || !wk.is_front(' ')){
				return false;
			}
			size_t next_space = wk.find(' ');
			if(next_space == std::string::npos){
				return false;
			}
			std::string request_target = wk.substr(0, next_space);
			if(!(request_target == "*" ? header.request_target.parse_uri_reference(request_target) : false) &&
				!header.request_target.parse_origin_form(request_target) &&
				!header.request_target.parse_absolute_uri(request_target) &&
				!header.request_target.parse_authority(request_target)){
				return false;
			}
			wk.advance(next_space + 1);
			if(!parse_http_version(header, wk))return false;
			if(!wk.is_front("\r\n"))return false;
			if(!wk.empty())return false;
			in = wk;
			return true;
		}
		static bool parse_status_line(http_header& header, input_type& in)
		{
			auto wk = in;
			if(!parse_http_version(header, wk))return false;
			if(!wk.is_front(' '))return false;
			std::string status_code;
			if(!wk.parse_token(status_code, url::is_digit, 3)) return false;
			if(status_code.size() != 3) return false;
			header.status_code = atoi(status_code.c_str());
			if(!wk.is_front(' '))return false;
			if(!wk.parse_string(header.reason_phrase, is_reason_phrase, false))return false;
			if(!wk.is_front("\r\n"))return false;
			if(!wk.empty())return false;
			in = wk;
			return true;
		}
		static bool is_tchar(char c)
		{
			static const std::string tchar = "!#$%&'*+-.^_`|~";
			return url::is_alpha(c) || url::is_digit(c) || tchar.find(c) != std::string::npos;
		}
		static bool is_vchar(char c)
		{
			return 0x21 <= c && c <= 0x7E;
		}
		static bool is_obs_text(char c)
		{
			uint8_t u = static_cast<uint8_t>(c);
			return 0x80 <= u;
		}
		static bool is_field_vchar(char c)
		{
			return is_vchar(c) || is_obs_text(c);
		}
		static bool is_field_value(char c)
		{
			return is_field_vchar(c) || is_ws(c);
		}
		static bool is_ws(char c)
		{
			return c == ' ' || c == '\t';
		}
		static bool is_reason_phrase(char c)
		{
			static const std::string rp = "\t ";
			return is_vchar(c) || is_obs_text(c) || rp.find(c) != std::string::npos;
		}
		static bool is_quoted_pair(char c)
		{
			return c == '\t' || c == ' ' || is_vchar(c) || is_obs_text(c);
		}
		static bool is_qdtext(char c)
		{
			return c == '\t' || c == ' ' || c == 0x21 || 
				(0x23 <= c && c <= 0x5B) ||
				(0x5D <= c && c <= 0x7E) ||
				is_obs_text(c);
		}
		static bool parse_quoted_pair(std::string& out, input_type& in)
		{
			input_type wk = in;
			if(wk.is_front('\\') && wk.is_front(is_quoted_pair)){
				out.push_back(in.at(1));
				in = wk;
				return true;
			}
			return false;
		}
		static bool parse_quoted_string(std::string& out, input_type& in)
		{
			input_type wk = in;
			if(wk.is_front('"')){
				while(!wk.is_front('"')){
					if(wk.empty()){
						return false;
					}
					char c = wk.front();
					if(wk.is_front(is_qdtext)){
						out.push_back(c);
						continue;
					}
					if(parse_quoted_pair(out, wk)){
						continue;
					}
					return false;
				}
				in = wk;
				return true;
			}
			return false;
		}
		static bool parse_token(std::string& token, input_type& in)
		{
			return in.parse_token(token, is_tchar, 1);
		}
		static bool parse_http_version(http_header& header, input_type& in)
		{
			auto wk = in;
			if(!wk.is_front("HTTP/"))return false;
			header.http_major_version = wk.front();
			if(!wk.is_front(url::is_digit))return false;
			if(!wk.is_front('.'))return false;
			header.http_minor_version = wk.front();
			if(!wk.is_front(url::is_digit))return false;
			in = wk;
			return true;
		}
		static bool parse_header_fields(http_header& header, input_type& in)
		{
			while(!in.empty()){
				auto line = get_line(in);
				if(line.size() < 2){
					return false;
				}
				if(line.size() == 2) {
					return true;
				}
				std::string field_name;
				if(!parse_token(field_name, line)){
					return false;
				}
				if(!line.is_front(':')){
					return false;
				}
				while(line.is_front(is_ws)); // OWS
				std::string field_value;
				if(!line.parse_field_content(field_value)){
					return false;
				}
				//field content includ after OWS
				while(!field_value.empty() && is_ws(field_value.back())){
					field_value.pop_back();
				}
				if(!line.is_front("\r\n"))return false;
				if(!line.empty())return false;
				field_name = header.normalize_field_name(field_name);
				header.headers[field_name].push_back(field_value);
			}
			return false;
		}
		static std::string to_lower(const std::string& value)
		{
			std::string result = value;
			std::transform(result.begin(), result.end(), result.begin(), [](char c) { return tolower(c); });
			return result;
		}
		static size_t to_hex(const std::string& str)
		{
			size_t result = 0;
			for(auto it = str.begin(), end = str.end(); it != end; ++it){
				if(std::numeric_limits<size_t>::max() / 16 <= result){
					return std::numeric_limits<size_t>::max();
				}
				result *= 16;
				size_t h = static_cast<size_t>(url::to_hex(*it));
				if(0xF < h || std::numeric_limits<size_t>::max() - h <= result){
					return std::numeric_limits<size_t>::max();
				}
				result += h;
			}
			return result;
		}
		static bool parse_chunk(std::vector<char>& body, input_type& in, bool& last)
		{
			//chunk = chunk-size [ chunk-ext ] CRLF chunk-data CRLF
			//last-chunk = 1*("0") [ chunk-ext ] CRLF
			//chunk-size = 1*HEXDIG
			std::string cs;
			auto wk = in;
			if(!wk.parse_token(cs, url::is_hex, 1)){
				return false;
			}
			last = (cs == "0");
			size_t chunk_size = to_hex(cs);
			if(chunk_size == std::numeric_limits<size_t>::max()){
				return false;
			}
			if((chunk_size == 0) != last){
				return false;
			}
			//chunk-ext = *( BWS ";" BWS chunk-ext-name [ BWS "=" BWS chunk-ext-val ] )
			while(wk.is_front(is_ws)); // BWS
			while(wk.is_front(';')){
				while(wk.is_front(is_ws)); // BWS
				//chunk-ext-name = token
				std::string name;
				if(!parse_token(name, wk)){
					return false;
				}
				while(wk.is_front(is_ws)); // BWS
				if(!wk.is_front('=')){
					continue;
				}
				while(wk.is_front(is_ws)); // BWS
				//chunk-ext-val  = token / quoted-string
				std::string value;
				if(!parse_token(value, wk) &&
					!parse_quoted_string(value, wk)){
					return false;
				}
				// ignore chunk-ext
				while(wk.is_front(is_ws)); // BWS
			}
			if(!wk.is_front("\r\n")){
				return false;
			}
			if(last){
				in = wk;
				return true;
			}
			if(wk.size() < chunk_size + 2){
				return true;
			}
			auto chunk_data = wk;
			wk.advance(chunk_size);
			if(!wk.is_front("\r\n")){
				return false;
			}
			body.insert(body.end(), chunk_data.begin, chunk_data.begin + chunk_size);
			in = wk;
			return true;
		}
		static bool parse_chunked_body(http_message& message, std::string& data)
		{
			if(data.empty()){
				return false;
			}
			input_type in(data.c_str(), data.c_str() + data.size());
			bool last = false;
			while(!last){
				auto line = get_line(in);
				if(line.empty()){
					break;
				}
				auto wk = in;
				if(!parse_chunk(message.request_body, wk, last)){
					message.response_header.reason_phrase = "Invalid chunk format";
					message.response_header.status_code = status_codes::BAD_REQUEST;
					return false;
				}
				if(wk.size() == in.size()){
					//not enough data
					break;
				}
				in = wk;
			}
			data.erase(data.begin(), data.begin() + data.size() - in.size());
			if(last){
				if(!message.request_header.transfer_encoding.end_chunk(message.request_body)){
					message.response_header.reason_phrase = "Invalid chunk format";
					message.response_header.status_code = status_codes::BAD_REQUEST;
					return false;
				}
				return true;
			}
			return false;
		}
	};
}
