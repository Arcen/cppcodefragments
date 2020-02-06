#include <ccfrag/http.h>
#include <vector>
#include <tuple>

bool rfc3986_5_4_reference_resolution_examples_test()
{
	ccfrag::url base("http://a/b/c/d;p?q");
	std::vector<std::tuple<std::string, std::string, bool> > examples = {
		// Normal Examples
		std::make_tuple("g:h", "g:h", true),
		std::make_tuple("g", "http://a/b/c/g", true),
		std::make_tuple("./g", "http://a/b/c/g", true),
		std::make_tuple("g/", "http://a/b/c/g/", true),
		std::make_tuple("/g", "http://a/g", true),
		std::make_tuple("//g", "http://g", true),
		std::make_tuple("?y", "http://a/b/c/d;p?y", true),
		std::make_tuple("g?y", "http://a/b/c/g?y", true),
		std::make_tuple("#s", "http://a/b/c/d;p?q#s", true),
		std::make_tuple("g#s", "http://a/b/c/g#s", true),
		std::make_tuple("g?y#s", "http://a/b/c/g?y#s", true),
		std::make_tuple(";x", "http://a/b/c/;x", true),
		std::make_tuple("g;x", "http://a/b/c/g;x", true),
		std::make_tuple("g;x?y#s", "http://a/b/c/g;x?y#s", true),
		std::make_tuple("", "http://a/b/c/d;p?q", true),
		std::make_tuple(".", "http://a/b/c/", true),
		std::make_tuple("./", "http://a/b/c/", true),
		std::make_tuple("..", "http://a/b/", true),
		std::make_tuple("../", "http://a/b/", true),
		std::make_tuple("../g", "http://a/b/g", true),
		std::make_tuple("../..", "http://a/", true),
		std::make_tuple("../../", "http://a/", true),
		std::make_tuple("../../g", "http://a/g", true),
		// Abnormal Examples
		std::make_tuple("../../../g", "http://a/g", true),
		std::make_tuple("../../../../g", "http://a/g", true),
		std::make_tuple("/./g", "http://a/g", true),
		std::make_tuple("/../g", "http://a/g", true),
		std::make_tuple("g.", "http://a/b/c/g.", true),
		std::make_tuple(".g", "http://a/b/c/.g", true),
		std::make_tuple("g..", "http://a/b/c/g..", true),
		std::make_tuple("..g", "http://a/b/c/..g", true),
		std::make_tuple("./../g", "http://a/b/g", true),
		std::make_tuple("./g/.", "http://a/b/c/g/", true),
		std::make_tuple("g/./h", "http://a/b/c/g/h", true),
		std::make_tuple("g/../h", "http://a/b/c/h", true),
		std::make_tuple("g;x=1/./y", "http://a/b/c/g;x=1/y", true),
		std::make_tuple("g;x=1/../y", "http://a/b/c/y", true),
		std::make_tuple("g?y/./x", "http://a/b/c/g?y/./x", true),
		std::make_tuple("g?y/../x", "http://a/b/c/g?y/../x", true),
		std::make_tuple("g#s/./x", "http://a/b/c/g#s/./x", true),
		std::make_tuple("g#s/../x", "http://a/b/c/g#s/../x", true),
		std::make_tuple("http:g", "http:g", true),
		std::make_tuple("http:g", "http://a/b/c/g", false)
	};
	for(auto it = examples.begin(), end = examples.end(); it != end; ++it){
		auto& example = *it;
		auto& relative = std::get<0>(example);
		auto& expected = std::get<1>(example);
		auto& strict = std::get<2>(example);
		ccfrag::url relative_url;
		//fprintf(stderr, "test set %s %s %d\n", relative.c_str(), expected.c_str(), strict);
		if(!relative_url.parse_uri_reference(relative)){
			fprintf(stderr, "parse_uri_reference(%s) error\n", relative.c_str());
			return false;
		}
		if(relative_url.get() != relative){
			fprintf(stderr, "relative url error %s != %s\n", relative_url.get().c_str(), relative.c_str());
			return false;
		}
		auto result = ccfrag::url::resolve(base, relative_url, strict);
		if(!result){
			fprintf(stderr, "resolve(%s,%s) error\n", base.get().c_str(), relative_url.get().c_str());
			return false;
		}
		if(result->get() != expected){
			fprintf(stderr, "resolve url error %s != %s\n", result->get().c_str(), expected.c_str());
			return false;
		}
	}
	return true;
}
bool rfc7230_test()
{
	ccfrag::url uri;
	return true;
}

bool uri_test()
{
	if(!rfc3986_5_4_reference_resolution_examples_test()){
		return false;
	}
	if(!rfc7230_test()){
		return false;
	}
	return true;
}

#include "test.h"
TEST(uri_test);
