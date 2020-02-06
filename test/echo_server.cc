#include <ccfrag/network.h>

class echo_server : public ccfrag::session
{
public:
	echo_server()
		: ccfrag::session()
	{
	}
	echo_server(int s)
		: ccfrag::session(s)
	{
		on_recv = std::bind(&echo_server::on_data, this);
	}
	bool on_data()
	{
		std::vector<char> all_data;
		for(auto it = read_buffer.begin(), end = read_buffer.end(); it != end; ++it){
			all_data.insert(all_data.end(), it->begin(), it->end());
		}
		read_buffer.clear();
		return send(all_data);
	}
	virtual std::shared_ptr<ccfrag::session> clone(int s)
	{
		return std::make_shared<echo_server>(s);
	}
};

int main(int argc, char *argv[])
{
	std::shared_ptr<ccfrag::session> server_session = std::make_shared<echo_server>();
	if(!server_session->open_tcp()){
		fprintf(stderr, "open error : %d, %s\n", errno, ccfrag::session::get_error_string().c_str());
		return -1;
	}
	std::deque<std::shared_ptr<ccfrag::session::socket_address> > bind_addresses;
	if(!ccfrag::session::socket_address::convert(bind_addresses, "0.0.0.0", "1024") || bind_addresses.empty()){
		fprintf(stderr, "convert error : %d, %s\n", errno, ccfrag::session::get_error_string().c_str());
		fprintf(stderr, "%zd\n", bind_addresses.size());
		return -1;
	}
	bool binded = false;
	for(auto it = bind_addresses.begin(), end = bind_addresses.end(); it != end; ++it){
		auto& addr = *it;
		if(!addr->is_ipv4()) continue;
		if(!server_session->bind(*addr)){
			fprintf(stderr, "bind error\n");
			return -1;
		}
		binded = true;
		break;
	}
	if(!binded){
		fprintf(stderr, "no bind addr error\n");
		return -1;
	}
	if(!server_session->listen(5)){
		fprintf(stderr, "listen error\n");
		return -1;
	}
	ccfrag::sessions server_sessions;
	if(!server_sessions.update(server_session.get())){
		fprintf(stderr, "epoll server error\n");
		return -1;
	}
	while(!server_session->is_closed()){
		server_sessions.process(-1);
	}
	return 0;
}