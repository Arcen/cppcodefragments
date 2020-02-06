#include <ccfrag/network.h>

bool network_test()
{
	ccfrag::sessions::initialize();
	{
		ccfrag::sessions ss;
		std::shared_ptr<ccfrag::session> s = std::make_shared<ccfrag::session>();
		std::shared_ptr<ccfrag::session> c = std::make_shared<ccfrag::session>();
		ccfrag::session::socket_address addr("0.0.0.0", "12345");
		if(!s->open_tcp()){
			return false;
		}
		if(!s->bind(addr)){
			return false;
		}
		if(!s->listen(5)){
			return false;
		}
		ss.update(s.get());
		ccfrag::session::socket_address target_addr("127.0.0.1", "12345");
		if(!c->open_tcp()){
			return false;
		}
		if(!c->connect(target_addr)){
			return false;
		}
		ss.update(c.get());
		int i = 100;
		while(i-- > 0){
			ss.process(10);
		}
		c.reset();
		s.reset();
	}
	ccfrag::sessions::finalize();
	return true;
}

#include "test.h"
TEST(network_test);
