#include <ccfrag/network.h>

class http_server : public ccfrag::session
{
public:
	http_server()
		: ccfrag::session()
	{
	}
	http_server(int s)
		: ccfrag::session(s)
	{
		on_recv = std::bind(&http_server::on_data, this);
	}
	bool on_data()
	{
		std::vector<char> all_data;
		if(read_buffer.size() > 1){
			for(auto it = read_buffer.begin(), end = read_buffer.end(); it != end; ++it){
				all_data.insert(all_data.end(), it->begin(), it->end());
			}
			read_buffer.clear();
			read_buffer.push_back(all_data);
		}
		return send(all_data);
	}
	virtual std::shared_ptr<ccfrag::session> clone(int s)
	{
		return std::make_shared<http_server>(s);
	}
};

int main(int argc, char *argv[])
{
	return 0;
}