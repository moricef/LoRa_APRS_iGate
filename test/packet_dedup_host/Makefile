CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -I../../include

.PHONY: all test clean

all: packet_dedup_test

packet_dedup_test: packet_dedup_test.cpp ../../src/packet_dedup.cpp ../../src/rxt_protocol.cpp ../../include/local_message_gate.h ../../include/packet_dedup.h ../../include/aprs_message_number.h
	$(CXX) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

test: packet_dedup_test
	./packet_dedup_test

clean:
	rm -f packet_dedup_test
