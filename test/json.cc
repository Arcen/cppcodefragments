#include <ccfrag/json.h>
#include <vector>
#include <string>

using namespace ccfrag;

bool test_json(const std::string& pattern, const std::string& normalized = std::string())
{
	auto v = json::json_value::parse(pattern);
	if(!v){
		fprintf(stderr, "parse error\n in ='%s'\n", pattern.c_str());
		return false;
	}
	std::string out = v->to_str();
	if(normalized.empty()){
		if(out != pattern){
			fprintf(stderr, "different\n in ='%s'\n out='%s'\n", pattern.c_str(), out.c_str());
			return false;
		}
	} else{
		if(out != normalized){
			fprintf(stderr, "different\n correct='%s'\n result ='%s'\n", normalized.c_str(), out.c_str());
			return false;
		}
	}
	return true;
}

bool test_patch(const std::string& target, const std::string& patch, const std::string& result, bool expect_evaluate = true, bool expect_parse_patch = true)
{
	auto json_target = json::json_value::parse(target);
	if(!json_target){
		fprintf(stderr, "target parse error %s\n", target.c_str());
		return false;
	}
	auto json_patch = json::json_value::parse(patch);
	if(!expect_parse_patch){
		if(!json_patch){
			return true;
		}
		fprintf(stderr, "patch parsed. but expect error %s\n", patch.c_str());
		return false;
	}
	if(!json_patch){
		fprintf(stderr, "patch parse error %s\n", patch.c_str());
		return false;
	}
	auto json_result = json::json_value::parse(result);
	if(!json_result){
		fprintf(stderr, "result parse error %s\n", result.c_str());
		return false;
	}
	auto patch_result = json::json_value::json_patch(json_target, json_patch);
	if(!patch_result){
		if(!expect_evaluate){
			return true;
		}
		fprintf(stderr, "patch error %s, expect=%d\n", patch.c_str(), expect_evaluate);
		fprintf(stderr, "target %s\n", json_target->to_str().c_str());
		fprintf(stderr, "patch %s\n", json_patch->to_str().c_str());
		fprintf(stderr, "expect %s\n", json_result->to_str().c_str());
		return false;
	}
	if(patch_result->equal(*json_result) != expect_evaluate){
		fprintf(stderr, "patch error %s, expect=%d\n", patch.c_str(), expect_evaluate);
		fprintf(stderr, "target %s\n", json_target->to_str().c_str());
		fprintf(stderr, "patch %s\n", json_patch->to_str().c_str());
		fprintf(stderr, "expect %s\n", json_result->to_str().c_str());
		fprintf(stderr, "result %s\n", patch_result->to_str().c_str());
		return false;
	}
	return true;
}

bool json_test()
{
	if(!test_json("[]")) return false;
	if(!test_json("{}")) return false;
	if(!test_json("[0]")) return false;
	if(!test_json("[0,1]")) return false;
	if(!test_json("[true]")) return false;
	if(!test_json("[false]")) return false;
	if(!test_json("[null]")) return false;
	if(!test_json("[-123.456e-10]")) return false;
	if(!test_json("[\"\"]")) return false;
	if(!test_json("[\"foo\"]")) return false;
	if(!test_json("[[[[]]]]")) return false;
	if(!test_json("[{},[],[],[[]],{}]")) return false;
	if(!test_json("{\"key\":null}")) return false;
	if(!test_json("{\"key\":[]}")) return false;
	if(!test_json("{\"key\":{}}")) return false;
	if(!test_json("{\"key\":\"value\"}")) return false;
	if(!test_json("{\"key\":\"value\",\"key2\":\"value\"}")) return false;
	if(!test_json(" [ \r0\n, \t1  ]", "[0,1]")) return false;
	if(!test_json("  {\n \"key\"\t: \"value\"\r\n,  \"key2\" : \"value\"\n}\n", "{\"key\":\"value\",\"key2\":\"value\"}")) return false;
	//RFC-exaples https://www.ietf.org/rfc/rfc4627.txt
	std::string rfc_example1 = 
	"{"
		"\"Image\":{"
			"\"Width\":800,"
			"\"Height\":600,"
			"\"Title\":\"View from 15th Floor\","
			"\"Thumbnail\":{"
				"\"Url\":\"http://www.example.com/image/481989943\","
				"\"Height\":125,"
				"\"Width\":\"100\""
			"},"
		"\"IDs\":[116,943,234,38793]}"
	"}";
	if(!test_json(rfc_example1)) return false;
	std::string rfc_example2 =
	"["
		"{"
			"\"precision\":\"zip\","
			"\"Latitude\":37.7668,"
			"\"Longitude\":-122.3959,"
			"\"Address\":\"\","
			"\"City\":\"SAN FRANCISCO\","
			"\"State\":\"CA\","
			"\"Zip\":\"94107\","
			"\"Country\":\"US\""
		"},"
		"{"
			"\"precision\":\"zip\","
			"\"Latitude\":37.371991,"
			"\"Longitude\":-122.026020,"
			"\"Address\":\"\","
			"\"City\":\"SUNNYVALE\","
			"\"State\":\"CA\","
			"\"Zip\":\"94085\","
			"\"Country\":\"US\""
		"}"
	"]";
	if(!test_json(rfc_example2)) return false;
	// string test
	if(!test_json("[\"\\\"\"]")) return false;
	if(!test_json("[\"\\\\\"]")) return false;
	if(!test_json("[\"\\/\"]", "[\"/\"]")) return false;
	if(!test_json("[\"\\b\"]")) return false;
	if(!test_json("[\"\\f\"]")) return false;
	if(!test_json("[\"\\n\"]")) return false;
	if(!test_json("[\"\\r\"]")) return false;
	if(!test_json("[\"\\t\"]")) return false;
	if(!test_json("[\"\\u0000\"]")) return false;
	if(!test_json("[\"\\u0020\"]", "[\" \"]")) return false;
	if(!test_json("[\"\\u000d\"]", "[\"\\r\"]")) return false;
	if(!test_json("[\"\\u000A\"]", "[\"\\n\"]")) return false;
	if(!test_json("[\"\\uD834\\udd1e\"]", "[\"\\ud834\\udd1e\"]")) return false;
	if(!test_json("[\"\\ud7ff\"]", "[\"\xED\x9F\xBF\"]")) return false;
	if(!test_json("[\"\\ue000\"]", "[\"\xEE\x80\x80\"]")) return false;
	if(!test_json("[\"\\ud800\\udc00\"]")) return false;
	if(!test_json("[\"\\udbff\\udfff\"]")) return false;
	// number test
	std::vector<std::string> signs = {"", "-"};
	std::vector<std::string> integer = {"0", "1", "123", "123456789", "987321"};
	std::vector<std::string> fraction = {"", ".1", ".123", ".123456789", ".0120"};
	std::vector<std::string> exponent = {"", "e-1", "E+0", "e123", "E-0"};
	for(auto its = signs.begin(); its != signs.end(); ++its){
		for(auto iti = integer.begin(); iti != integer.end(); ++iti){
			for(auto itf = fraction.begin(); itf != fraction.end(); ++itf){
				for(auto ite = exponent.begin(); ite != exponent.end(); ++ite){
					if(!test_json("[" + *its + *iti + *itf + *ite + "]")) return false;
				}
			}
		}
	}

	// json merge patch test
	// https://tools.ietf.org/html/rfc7396
	std::string rfc7396_base = R"(
{
  "title": "Goodbye!",
  "author" : {
    "givenName" : "John",
    "familyName" : "Doe"
  },
  "tags":[ "example", "sample" ],
  "content": "This will be unchanged"
}
	)";
	std::string rfc7396_patch = R"(
{
  "title": "Hello!",
  "phoneNumber": "+01-123-456-7890",
  "author": {
    "familyName": null
  },
  "tags": [ "example" ]
}
	)";
	std::string rfc7396_result = R"(
{
  "title": "Hello!",
  "author" : {
    "givenName" : "John"
  },
  "tags": [ "example" ],
  "content": "This will be unchanged",
  "phoneNumber": "+01-123-456-7890"
}
	)";
	json::json_value j_base;
	json::json_value j_patch;
	json::json_value j_result;
	if(!j_base.parse(rfc7396_base)){
		return false;
	}
	if(!j_patch.parse(rfc7396_patch)){
		return false;
	}
	if(!j_result.parse(rfc7396_result)){
		return false;
	}
	auto patched = json::json_value::merge_patch(j_base.clone(), j_patch.clone());
	if(!patched){
		return false;
	}
	if(!patched->equal(j_result)){
		printf("%s\n", patched->to_str().c_str());
		printf("%s\n", j_result.to_str().c_str());
		return false;
	}

	// json patch test
	// https://tools.ietf.org/html/rfc6902
	std::string rfc6902_a1_target = R"({ "foo": "bar"})";
	std::string rfc6902_a1_patch = R"([
		{ "op": "add", "path": "/baz", "value": "qux" }
	])";
	std::string rfc6902_a1_result = R"({"baz": "qux","foo": "bar"})";
	if(!test_patch(rfc6902_a1_target, rfc6902_a1_patch, rfc6902_a1_result)) return false;

	std::string rfc6902_a2_target = R"({ "foo": [ "bar", "baz" ] })";
	std::string rfc6902_a2_patch = R"([
		{ "op": "add", "path": "/foo/1", "value": "qux" }
	])";
	std::string rfc6902_a2_result = R"({ "foo": [ "bar", "qux", "baz" ] })";
	if(!test_patch(rfc6902_a2_target, rfc6902_a2_patch, rfc6902_a2_result)) return false;

	std::string rfc6902_a3_target = R"({"baz": "qux","foo": "bar"})";
	std::string rfc6902_a3_patch = R"([
		{ "op": "remove", "path": "/baz" }
	])";
	std::string rfc6902_a3_result = R"({ "foo": "bar" })";
	if(!test_patch(rfc6902_a3_target, rfc6902_a3_patch, rfc6902_a3_result)) return false;

	std::string rfc6902_a4_target = R"({ "foo": [ "bar", "qux", "baz" ] })";
	std::string rfc6902_a4_patch = R"([
		{ "op": "remove", "path": "/foo/1" }
	])";
	std::string rfc6902_a4_result = R"({ "foo": [ "bar", "baz" ] })";
	if(!test_patch(rfc6902_a4_target, rfc6902_a4_patch, rfc6902_a4_result)) return false;

	std::string rfc6902_a5_target = R"(  {"baz": "qux","foo": "bar"})";
	std::string rfc6902_a5_patch = R"([
		{ "op": "replace", "path": "/baz", "value": "boo" }
	])";
	std::string rfc6902_a5_result = R"({"baz": "boo","foo": "bar"})";
	if(!test_patch(rfc6902_a5_target, rfc6902_a5_patch, rfc6902_a5_result)) return false;

	std::string rfc6902_a6_target = R"({"foo": {"bar": "baz","waldo": "fred"},"qux": {"corge": "grault"}})";
	std::string rfc6902_a6_patch = R"([
		{ "op": "move", "from": "/foo/waldo", "path": "/qux/thud" }
	])";
	std::string rfc6902_a6_result = R"({"foo": {"bar": "baz"},"qux": {"corge": "grault","thud": "fred"}})";
	if(!test_patch(rfc6902_a6_target, rfc6902_a6_patch, rfc6902_a6_result)) return false;

	std::string rfc6902_a7_target = R"( { "foo": [ "all", "grass", "cows", "eat" ] })";
	std::string rfc6902_a7_patch = R"([
		{ "op": "move", "from": "/foo/1", "path": "/foo/3" }
	])";
	std::string rfc6902_a7_result = R"({ "foo": [ "all", "cows", "eat", "grass" ] })";
	if(!test_patch(rfc6902_a7_target, rfc6902_a7_patch, rfc6902_a7_result)) return false;

	std::string rfc6902_a8_target = R"({"baz": "qux","foo": [ "a", 2, "c" ]})";
	std::string rfc6902_a8_patch = R"([
		{ "op": "test", "path": "/baz", "value": "qux" },
		{ "op": "test", "path": "/foo/1", "value": 2 }
	])";
	if(!test_patch(rfc6902_a8_target, rfc6902_a8_patch, rfc6902_a8_target)) return false;

	std::string rfc6902_a9_target = R"({ "baz": "qux" })";
	std::string rfc6902_a9_patch = R"([
		{ "op": "test", "path": "/baz", "value": "bar" }
	])";
	if(!test_patch(rfc6902_a9_target, rfc6902_a9_patch, rfc6902_a9_target, false)) return false;

	std::string rfc6902_a10_target = R"({ "foo": "bar" })";
	std::string rfc6902_a10_patch = R"([
		{ "op": "add", "path": "/child", "value": { "grandchild": { } } }
	])";
	std::string rfc6902_a10_result = R"({"foo": "bar","child": {"grandchild": {}}})";
	if(!test_patch(rfc6902_a10_target, rfc6902_a10_patch, rfc6902_a10_result)) return false;

	std::string rfc6902_a11_target = R"({ "foo": "bar" })";
	std::string rfc6902_a11_patch = R"([
		{ "op": "add", "path": "/baz", "value": "qux", "xyz": 123 }
	])";
	std::string rfc6902_a11_result = R"({"foo": "bar","baz": "qux"})";
	if(!test_patch(rfc6902_a11_target, rfc6902_a11_patch, rfc6902_a11_result)) return false;

	std::string rfc6902_a12_target = R"({ "foo": "bar" })";
	std::string rfc6902_a12_patch = R"([
		{ "op": "add", "path": "/baz/bat", "value": "qux" }
	])";
	if(!test_patch(rfc6902_a12_target, rfc6902_a12_patch, rfc6902_a12_target, false)) return false;

	std::string rfc6902_a13_target = R"({})";
	std::string rfc6902_a13_patch = R"([
		{ "op": "add", "path": "/baz", "value": "qux", "op": "remove" }
	])";
	if(!test_patch(rfc6902_a13_target, rfc6902_a13_patch, rfc6902_a13_target, false, false)) return false;

	std::string rfc6902_a14_target = R"({"/": 9,"~1": 10})";
	std::string rfc6902_a14_patch = R"([
		{"op": "test", "path": "/~01", "value": 10}
	])";
	std::string rfc6902_a14_result = R"({"/": 9,"~1": 10})";
	if(!test_patch(rfc6902_a14_target, rfc6902_a14_patch, rfc6902_a14_result)) return false;

	std::string rfc6902_a15_target = R"({"/": 9,"~1": 10})";
	std::string rfc6902_a15_patch = R"([
		{"op": "test", "path": "/~01", "value": "10"}
	])";
	if(!test_patch(rfc6902_a15_target, rfc6902_a15_patch, rfc6902_a15_target, false)) return false;

	std::string rfc6902_a16_target = R"({ "foo": ["bar"] })";
	std::string rfc6902_a16_patch = R"([
		{ "op": "add", "path": "/foo/-", "value": ["abc", "def"] }
	])";
	std::string rfc6902_a16_result = R"({ "foo": ["bar", ["abc", "def"]] })";
	if(!test_patch(rfc6902_a16_target, rfc6902_a16_patch, rfc6902_a16_result)) return false;

	return true;
}

#include "test.h"
TEST(json_test);
