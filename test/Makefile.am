TESTS = json network uri compress.sh gzip
noinst_PROGRAMS = echo_server http_server compress
AM_CXXFLAGS=-I../include -std=c++11

check_PROGRAMS = json network uri
json_SOURCES = json.cc
network_SOURCES = network.cc
uri_SOURCES = uri.cc
gzip_SOURCES = gzip.cc

echo_server_SOURCES = echo_server.cc
http_server_SOURCES = http_server.cc
compress_SOURCES = compress.cc
