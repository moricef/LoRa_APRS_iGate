CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -I../../include

.PHONY: all test clean

all: packet_dedup_test

packet_dedup_test: packet_dedup_test.cpp ../../src/packet_dedup.cpp ../../src/rxt_protocol.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

test: packet_dedup_test
	./packet_dedup_test

clean:
	rm -f packet_dedup_test
