#pragma once
#include <string>
#include <deque>
#include <vector>
#include <list>
#include <map>
#include <memory>
#include <functional>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_CONFIG_H
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#else
#include <mutex>
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

namespace ccfrag{
	class session : public std::enable_shared_from_this<session>
	{
	public:
#ifdef HAVE_CONFIG_H
		typedef int socket_t;
#else
		typedef SOCKET socket_t;
#endif
	private:
		socket_t fd;
		bool listening;
	public:
		typedef std::function<bool()> callback_function_type;
		callback_function_type on_close; // set from epoll to del
		callback_function_type on_send; // set from epoll to mod EPOLLOUT
		callback_function_type on_recv; // for data coming
		std::deque<std::vector<char> > read_buffer;
		std::deque<std::vector<char> > write_buffer;
		std::weak_ptr<session> parent;
		std::list<std::shared_ptr<session> > children;
		class socket_address{
			struct sockaddr_storage value;
		public:
#ifndef HAVE_CONFIG_H
			typedef int socklen_t;
#endif
			socket_address()
			{
				memset(&value, 0, sizeof(value));
			}
			socket_address(const std::string& node, const std::string& service)
			{
				memset(&value, 0, sizeof(value));
				std::deque<std::shared_ptr<socket_address> > addrs;
				if(convert(addrs, node, service)){
					*this = *addrs.front();
				}
			}
			socket_address(sockaddr *addr, socklen_t addrlen)
			{
				if(addr && addrlen <= sizeof(value)){
					memcpy(&value, addr, addrlen);
				}else{
					memset(&value, 0, sizeof(value));
				}
			}
			int size() const
			{
				if(is_ipv4()) return static_cast<int>(sizeof(struct sockaddr_in));
				if(is_ipv6()) return static_cast<int>(sizeof(struct sockaddr_in6));
				return static_cast<int>(sizeof(value));
			}
			struct sockaddr * ptr() { return reinterpret_cast<struct sockaddr *>(&value); }
			const struct sockaddr * ptr() const { return reinterpret_cast<const struct sockaddr *>(&value); }
			bool is_ipv4() const { return (value.ss_family == AF_INET); }
			bool is_ipv6() const { return (value.ss_family == AF_INET6); }
			struct sockaddr_in& as_ipv4()
			{
				return *reinterpret_cast<sockaddr_in*>(&value);
			}
			const struct sockaddr_in& as_ipv4() const
			{
				return *reinterpret_cast<const sockaddr_in*>(&value);
			}
			struct sockaddr_in6& as_ipv6()
			{
				return *reinterpret_cast<sockaddr_in6*>(&value);
			}
			const struct sockaddr_in6& as_ipv6() const
			{
				return *reinterpret_cast<const sockaddr_in6*>(&value);
			}
			bool set_port(uint16_t port)
			{
				if(is_ipv4()){
					as_ipv4().sin_port = htons(port);
					return true;
				}
				if(is_ipv6()){
					as_ipv6().sin6_port = htons(port);
					return true;
				}
				return false;
			}
			static bool convert(std::deque<std::shared_ptr<socket_address> > & addrs, const std::string& node, const std::string& service)
			{
				struct addrinfo *result = nullptr;
				int eai_error = getaddrinfo(node.c_str(), service.c_str(), nullptr, &result);
				if(eai_error){
					fprintf(stderr, "getaddrinfo error %s\n", ::gai_strerror(eai_error));
					return false;
				}
				bool ok = false;
				for(struct addrinfo *it = result; it; it = it->ai_next){
					if(sizeof(sockaddr_storage) < it->ai_addrlen){
						continue;
					}
					std::shared_ptr<socket_address> addr(new socket_address(it->ai_addr, static_cast<int>(it->ai_addrlen)));
					addrs.push_back(addr);
				}
				::freeaddrinfo(result);
				return !addrs.empty();
			}
		};
		static bool is_error(int r)
		{
#ifdef HAVE_CONFIG_H
			return r < 0;
#else
			return r != 0;
#endif
		}
		static bool is_invalid(socket_t r)
		{
#ifdef HAVE_CONFIG_H
			return r < 0;
#else
			return r == INVALID_SOCKET;
#endif
		}
		static socket_t invalid_socket()
		{
#ifdef HAVE_CONFIG_H
			return -1;
#else
			return INVALID_SOCKET;
#endif
		}
		static bool is_blocked()
		{
#ifdef HAVE_CONFIG_H
			int e = errno;
			return (e == EAGAIN || e == EWOULDBLOCK);
#else
			auto e = WSAGetLastError();
			return (e == WSAEWOULDBLOCK);
#endif
		}
		static bool is_connecting()
		{
#ifdef HAVE_CONFIG_H
			int e = errno;
			return (e == EINPROGRESS);
#else
			auto e = WSAGetLastError();
			return (e == WSAEWOULDBLOCK);
#endif
		}
		static bool is_interrupted()
		{
#ifdef HAVE_CONFIG_H
			int e = errno;
			return (e == EINTR);
#else
			auto e = WSAGetLastError();
			return (e == WSAEINTR);
#endif
		}
		static std::string get_error_string()
		{
#ifdef HAVE_CONFIG_H
			int e = errno;
			std::vector<char> wk(1024, '\0');
			return std::string(strerror_r(e, &wk[0], 1023));
#else
			auto e = WSAGetLastError();
			LPVOID buf;
			auto r = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, NULL);
			std::string msg((LPCSTR) buf);
			LocalFree(buf);
			return msg;
#endif
		}
		session()
		: fd(invalid_socket())
		, listening(false)
		{
		}
		session(socket_t fd)
		: fd(fd)
		, listening(false)
		{
		}
		virtual ~session()
		{
			close();
		}
		socket_t get_fd() const
		{
			return fd;
		}
		socket_t release_fd()
		{
			socket_t r = fd;
			fd = invalid_socket();
			return r;
		}
		void set_fd(socket_t fd)
		{
			close();
			this->fd = fd;
		}
		static void close(socket_t s)
		{
#ifdef HAVE_CONFIG_H
			::close(s);
#else
			closesocket(s);
#endif
		}
		void close()
		{
			if(!is_invalid(fd)){
				if(on_close) {
					on_close();
					on_close = nullptr;
				}
				close(fd);
				fd = invalid_socket();
				if(auto listen_session = parent.lock()){
					auto self = shared_from_this();
					if(self){
						listen_session->children.remove(self);
					}
				}
			}
		}
		bool is_listen() const { return listening; }
		bool open(int family, int type, int protocol)
		{
			close();
			fd = ::socket(family, type, protocol);
			if(is_invalid(fd)){
				fprintf(stderr, "socket : %s\n", session::get_error_string().c_str());
				return false;
			}
			return true;
		}
		bool is_closed() const { return is_invalid(fd); }
		bool set_nonblock(bool nonblock)
		{
#ifdef HAVE_CONFIG_H
			int val = (nonblock ? 1 : 0);
			auto r = ioctl(get_fd(), FIONBIO, &val);
#else
			u_long val = (nonblock ? 1 : 0);
			auto r = ioctlsocket(get_fd(), FIONBIO, &val);
#endif
			if(is_error(r)){
				return false;
			}
			return true;
		}
		bool open_tcp(bool nonblock = true, bool ipv4 = true)
		{
#ifdef HAVE_CONFIG_H
			return open(ipv4 ? AF_INET : AF_INET6, SOCK_STREAM | SOCK_CLOEXEC | (nonblock ? SOCK_NONBLOCK : 0), 0);
#else
			if(!open(ipv4 ? AF_INET : AF_INET6, SOCK_STREAM, 0)){
				return false;
			}
			if(nonblock && !set_nonblock(nonblock)){
				return false;
			}
			return true;
#endif
		}
		bool open_udp(bool nonblock = true, bool ipv4 = true)
		{
#ifdef HAVE_CONFIG_H
			return open(ipv4 ? AF_INET : AF_INET6, SOCK_DGRAM | SOCK_CLOEXEC | (nonblock ? SOCK_NONBLOCK : 0), 0);
#else
			if(!open(ipv4 ? AF_INET : AF_INET6, SOCK_DGRAM, 0)){
				return false;
			}
			if(nonblock && !set_nonblock(nonblock)){
				return false;
			}
			return true;
#endif
		}
		bool bind(const socket_address& addr)
		{
#ifndef HAVE_CONFIG_H
			BOOL yes = 1;
			setsockopt(get_fd(), SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
#endif
			int r = ::bind(get_fd(), addr.ptr(), addr.size());
			if(is_error(r)){
				fprintf(stderr, "bind : %s\n", session::get_error_string().c_str());
				return false;
			}
			return true;
		}
		bool connect(const socket_address& addr)
		{
			int r = ::connect(get_fd(), addr.ptr(), addr.size());
			if(is_error(r)){
				if(is_connecting()){
					return true;
				}
				fprintf(stderr, "connect : %d, %s\n", errno, session::get_error_string().c_str());
				return false;
			}
			return true;
		}
		bool listen(int backlog)
		{
			int r = ::listen(get_fd(), backlog);
			if(is_error(r)){
				return false;
			}
			listening = true;
			return true;
		}
		virtual std::shared_ptr<session> clone(socket_t s)
		{
			return std::make_shared<session>(s);
		}
		bool accept(std::shared_ptr<session>& result, bool nonblock = true)
		{
#ifdef HAVE_CONFIG_H
			socket_t r = ::accept4(get_fd(), nullptr, nullptr, SOCK_CLOEXEC | (nonblock ? SOCK_NONBLOCK : 0));
#else
			socket_t r = ::accept(get_fd(), nullptr, nullptr);
#endif
			if(is_invalid(r)){
				if(is_blocked()){
					return true;
				}
				return false;
			}
			result = clone(r);
			if(!result){
				close(r);
				return false;
			}
			result->parent = shared_from_this();
			children.push_back(result);
			return true;
		}
		bool has_write_data() const
		{
			for(auto it = write_buffer.begin(), end = write_buffer.end(); it != end; ++it){
				if(!it->empty()) return true;
			}
			return false;
		}
		bool send(const std::vector<char>& data)
		{
			if(is_closed()) return false;
			if(data.empty()) return true;
			write_buffer.push_back(data);
			if(write_buffer.size() == 1){
				if(!on_can_send()){
					return false;
				}
			}
			if(on_send) on_send();
			return true;
		}
		bool on_can_send()
		{
			if(is_closed()) return false;
			while(!write_buffer.empty()){
				auto& front_buffer = write_buffer.front();
				if(front_buffer.empty()){
					write_buffer.pop_front();
					continue;
				}
				const char * ptr = front_buffer.data();
				size_t size = front_buffer.size();
				size_t sent_size = 0;
				while(size){
					int flags = 0;
#ifdef HAVE_CONFIG_H
					flags = (write_buffer.size() > 1 ? MSG_MORE : 0);
#endif
					int r = ::send(get_fd(), ptr, static_cast<int>(size), flags);
					if(r < 0){
						if(is_blocked()){
							if(sent_size){
								front_buffer.erase(front_buffer.begin(), front_buffer.begin() + sent_size);
							}
							return true;
						}
						if(is_interrupted()){
							continue;
						}
						return false;
					}
					if(size <= r){
						break;
					}
					ptr += r;
					size -= r;
					sent_size += r;
				}
				write_buffer.pop_front();
			}
			return true;
		}
		bool on_can_recv()
		{
			std::vector<char> buffer;
			buffer.resize(1024);
			while(true){
				int r = ::recv(get_fd(), &buffer[0], static_cast<int>(buffer.size()), 0);
				if(r == 0){
					close();
					return true;
				}
				if(r < 0){
					if(is_blocked()){
						if(on_recv) on_recv();
						return true;
					}
					if(is_interrupted()){
						continue;
					}
					return false;
				}
				read_buffer.push_back(std::vector<char>(buffer.begin(), buffer.begin() + r));
			}
		}
	};
#ifdef HAVE_CONFIG_H
	typedef int epoll_t;
#define epoll_close(fd);	::close(fd);
#else
	enum{
		EPOLL_CTL_ADD,
		EPOLL_CTL_MOD,
		EPOLL_CTL_DEL,
		EPOLL_CLOEXEC,
		EPOLLIN = (1 << 0),
		EPOLLOUT = (1 << 1),
		EPOLLRDHUP = (1 << 2),
		EPOLLPRI = (1 << 3),
		EPOLLERR = (1 << 4),
		EPOLLHUP = (1 << 5),
		EPOLLET = (1 << 6),
		EPOLLONECHOST = (1 << 7),
	};
	typedef union epoll_data{
		void    *ptr;
		int      fd;
		uint32_t u32;
		uint64_t u64;
	} epoll_data_t;
	struct epoll_event{
		uint32_t events;
		epoll_data_t data;
		DWORD get_wsa_event() const
		{
			DWORD result = 0;
			if(events&EPOLLIN) result |= (FD_READ | FD_ACCEPT | FD_CLOSE);
			if(events&EPOLLOUT) result |= (FD_WRITE | FD_CONNECT | FD_CLOSE);
			if(events&EPOLLRDHUP) result |= FD_CLOSE;
			if(events&EPOLLPRI) result |= FD_OOB;
			if(events&EPOLLHUP) result |= FD_CLOSE;
			return result;
		}
		void set_wsa_event(DWORD wsa_event)
		{
			events = 0;
			if(wsa_event&(FD_READ | FD_ACCEPT)) events |= EPOLLIN;
			if(wsa_event&(FD_WRITE | FD_CONNECT)) events |= EPOLLOUT;
			if(wsa_event&FD_CLOSE) events |= EPOLLIN | EPOLLHUP;
			if(wsa_event&FD_OOB) events |= EPOLLPRI;
		}
	};
	class epoll{
		std::mutex mutex;
		std::map<session::socket_t, size_t> socket_to_index;
		std::map<size_t, session::socket_t> index_to_socket;
		std::vector<WSAEVENT> wsa_events;
		std::vector<epoll_event> epoll_events;
	public:
		epoll()
		{
		}
		int control(int op, session::socket_t fd, epoll_event *event)
		{
			switch(op){
			case EPOLL_CTL_ADD:
				return control_add(fd, event);
			case EPOLL_CTL_MOD:
				return control_mod(fd, event);
			case EPOLL_CTL_DEL:
				return control_del(fd, event);
			}
			errno = EINVAL;
			return -1;
		}
		int wait(epoll_event *events, int maxevents, int timeout)
		{
			if(maxevents < 1){
				errno = EINVAL;
				return -1;
			}
			std::lock_guard<std::mutex> lock(mutex);
			if(wsa_events.empty()){
				errno = EINVAL;
				return -1;
			}
			DWORD index = WSAWaitForMultipleEvents(
				static_cast<DWORD>(wsa_events.size()),
				&wsa_events[0],
				FALSE, // NO wait all
				static_cast<DWORD>(timeout < 0 ? WSA_INFINITE : timeout), // milli seconds
				FALSE); // wait for I/O completion routines
			if(index == WSA_WAIT_TIMEOUT){
				return 0;
			}
			if(WSA_WAIT_EVENT_0 <= index && index < WSA_WAIT_EVENT_0 + wsa_events.size()){
				index -= WSA_WAIT_EVENT_0;
				WSAResetEvent(wsa_events[index]);
				auto socket = index_to_socket[index];
				auto event = wsa_events[index];
				WSANETWORKEVENTS network_events;
				WSAEnumNetworkEvents(socket, event, &network_events);
				events[0] = epoll_events[index];
				events[0].set_wsa_event(network_events.lNetworkEvents);
				return 1;
			}
			if(index == WSA_WAIT_FAILED){
				errno = EINVAL;
				return -1;
			}
			if(index == WSA_WAIT_IO_COMPLETION){
				errno = EINTR;// I/O completion ended, check again now
				return -1;
			}
			errno = EINVAL;
			return -1;
		}
		int control_add(session::socket_t fd, epoll_event *event)
		{
			if(!event){
				errno = EINVAL;
				return -1;
			}
			std::lock_guard<std::mutex> lock(mutex);
			auto it = socket_to_index.find(fd);
			if(it != socket_to_index.end()){
				errno = EEXIST;
				return -1;
			}
			WSAEVENT event_object = WSACreateEvent();
			if(event_object == WSA_INVALID_EVENT){
				errno = ENOSPC;
				return -1;
			}
			if(WSAEventSelect(fd, event_object, event->get_wsa_event()) == SOCKET_ERROR){
				WSACloseEvent(event_object);
				errno = EINVAL;
				return -1;
			}
			size_t index = wsa_events.size();
			wsa_events.push_back(event_object);
			epoll_events.push_back(*event);
			socket_to_index[fd] = index;
			index_to_socket[index] = fd;
			return 0;
		}
		int control_mod(session::socket_t fd, epoll_event *event)
		{
			if(!event){
				errno = EINVAL;
				return -1;
			}
			std::lock_guard<std::mutex> lock(mutex);
			auto it = socket_to_index.find(fd);
			if(it == socket_to_index.end()){
				errno = ENOENT;
				return -1;
			}
			size_t index = it->second;
			if(wsa_events.size() <= index){
				errno = ENOENT;
				return -1;
			}
			WSAEVENT event_object = wsa_events[index];
			if(WSAEventSelect(fd, event_object, event->get_wsa_event()) == SOCKET_ERROR){
				errno = EINVAL;
				return -1;
			}
			epoll_events[index] = *event;
			return 0;
		}
		int control_del(session::socket_t fd, epoll_event *event)
		{
			if(!event){
				errno = EINVAL;
				return -1;
			}
			std::lock_guard<std::mutex> lock(mutex);
			auto it = socket_to_index.find(fd);
			if(it == socket_to_index.end()){
				errno = ENOENT;
				return -1;
			}
			size_t index = it->second;
			if(wsa_events.size() <= index){
				errno = ENOENT;
				return -1;
			}
			WSAEVENT event_object = wsa_events[index];
			if(WSAEventSelect(fd, NULL, 0) == SOCKET_ERROR){
				errno = EINVAL;
				return -1;
			}
			WSACloseEvent(event_object);
			if(index + 1 < wsa_events.size()){
				size_t target_index = wsa_events.size() - 1;
				session::socket_t target_socket = index_to_socket[target_index];
				index_to_socket[index] = index_to_socket[target_index];
				socket_to_index[target_socket] = index;
				std::swap(wsa_events[index], wsa_events.back());
				std::swap(epoll_events[index], epoll_events.back());
			}
			wsa_events.pop_back();
			epoll_events.pop_back();
			index_to_socket.erase(wsa_events.size());
			socket_to_index.erase(it);
			return 0;
		}
	};
	typedef epoll *epoll_t;
	inline epoll_t epoll_create1(int){ return new epoll(); }
	inline int epoll_close(epoll_t& e){ if(e) delete e; e = nullptr; return 0; }
	inline int epoll_ctl(epoll_t epfd, int op, session::socket_t fd, epoll_event *event){ return epfd->control(op, fd, event); }
	inline int epoll_wait(epoll_t epfd, epoll_event *events, int maxevents, int timeout)
	{
		if(!epfd){
			errno = EBADF;
			return -1;
		}
		return epfd->wait(events, maxevents, timeout);
	}
#endif
	class sessions
	{
	public:
		typedef session::socket_t socket_t;
	private:
		epoll_t fd;
	public:
		static bool initialize()
		{
#ifndef HAVE_CONFIG_H
			WSADATA wsadata;
			auto r = WSAStartup(MAKEWORD(2,0), &wsadata);
			if(r){
				switch(r){
				case WSASYSNOTREADY:
					return false;
				case WSAVERNOTSUPPORTED:
					return false;
				case WSAEINPROGRESS:
					return false;
				case WSAEPROCLIM:
					return false;
				case WSAEFAULT:
					return false;
				}
			}
#endif
			return true;
		}
		static bool finalize()
		{
#ifndef HAVE_CONFIG_H
			WSACleanup();
#endif
			return true;
		}
		sessions()
		: fd(epoll_create1(EPOLL_CLOEXEC))
		{
		}
		virtual ~sessions()
		{
			close();
		}
		void close()
		{
#ifdef HAVE_CONFIG_H
			if(!session::is_invalid(fd)){
				session::close(fd);
				fd = session::invalid_socket();
			}
#else
			epoll_close(fd);
#endif
		}
		bool update(session* s)
		{
			if(!s || s->is_closed()) return false;
			s->on_close = std::bind(&sessions::del, this, s);
			s->on_send = std::bind(&sessions::update, this, s);
			uint32_t events = EPOLLIN | (s->has_write_data() ? EPOLLOUT : 0);
			bool r = mod(s, events);
			if(!r && errno == ENOENT){
				r = add(s, events);
			}
			return r;
		}
		bool process(int timeout_millisec, int retry_count = 5, size_t one_time_event_count = 100)
		{
			std::vector<struct epoll_event> events;
			events.resize(one_time_event_count);
			while(0 < retry_count--){
				int r = epoll_wait(fd, &events[0], static_cast<int>(events.size()), timeout_millisec);
				if(r == 0){//timeout
					return true;
				}
				if(r < 0){
					if(session::is_interrupted()){
						continue;
					}
					return false;
				}
				for(int i = 0; i < r; ++i){
					auto& event = events[i];
					session * s = reinterpret_cast<session*>(event.data.ptr);
					if(!s){
						continue;
					}
					if(event.events & EPOLLIN){
						if(s->is_listen()){
							while(true){
								std::shared_ptr<session> ss;
								if(s->accept(ss)){
									if(ss){
										update(ss.get());
									}else{
										break;
									}
								}else{
									s->close();
								}
							}
						}else{
							s->on_can_recv();
						}
					}
					if(event.events & EPOLLOUT){
						s->on_can_send();
					}
				}
			}
			return true;
		}
	private:
		bool add(session* s, uint32_t events)
		{
			if(!s || s->is_closed()) return false;
			struct epoll_event ee;
			memset(&ee, 0, sizeof(ee));
			ee.events = events;
			ee.data.ptr = s;
			int r = epoll_ctl(fd, EPOLL_CTL_ADD, s->get_fd(), &ee);
			if(r < 0){
				return false;
			}
			return true;
		}
		bool mod(session* s, uint32_t events)
		{
			if(!s || s->is_closed()) return false;
			struct epoll_event ee;
			memset(&ee, 0, sizeof(ee));
			ee.events = events;
			ee.data.ptr = s;
			int r = epoll_ctl(fd, EPOLL_CTL_MOD, s->get_fd(), &ee);
			if(r < 0){
				return false;
			}
			return true;
		}
		bool del(session* s)
		{
			if(!s || s->is_closed()) return false;
			struct epoll_event ee;
			memset(&ee, 0, sizeof(ee));
			ee.data.ptr = s;
			int r = epoll_ctl(fd, EPOLL_CTL_DEL, s->get_fd(), &ee);
			if(r < 0){
				return false;
			}
			return true;
		}
	};
}
