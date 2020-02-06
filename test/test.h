#include <functional>
#include <list>
class test_framework{
public:
	typedef std::function<bool ()> test_function;
	static std::list<test_function>& all_tests()
	{
		static std::list<test_function> tests;
		return tests;
	}
	test_framework(test_function func)
	{
		all_tests().push_back(func);
	}
	static bool execute()
	{
		auto& tests = all_tests();
		for(auto it = tests.begin(), end = tests.end(); it != end; ++it){
			if(!(*it)()) return false;
		}
		return true;
	}
};
#ifdef HAVE_CONFIG_H
#define TEST(name) int main(){ return name() ? 0 : -1; }
#else
#define TEST(name) static test_framework test(name);
#endif
