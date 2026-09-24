CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -I../../include

.PHONY: all test clean

all: rxt_host_test

rxt_host_test: rxt_host_test.cpp ../../src/rxt_protocol.cpp ../../src/aprs_telemetry_rx.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

test: rxt_host_test
	./rxt_host_test

clean:
	rm -f rxt_host_test
