CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Werror

native_aprs_tool: native_aprs_tool.cpp ../../src/native_aprs.cpp ../../src/rxt_protocol.cpp ../../include/native_aprs.h ../../include/native_aprs_huffman_table.h
	$(CXX) $(CXXFLAGS) -I../../include -o $@ native_aprs_tool.cpp ../../src/native_aprs.cpp ../../src/rxt_protocol.cpp

clean:
	$(RM) native_aprs_tool
