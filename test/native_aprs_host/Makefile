CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -I../../include

.PHONY: all test clean

all: native_aprs_host_test

native_aprs_host_test: native_aprs_host_test.cpp ../../src/native_aprs.cpp ../../src/rxt_protocol.cpp ../../include/native_aprs.h ../../include/native_aprs_huffman_table.h
	$(CXX) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

test: native_aprs_host_test
	./native_aprs_host_test vectors.txt

clean:
	rm -f native_aprs_host_test
