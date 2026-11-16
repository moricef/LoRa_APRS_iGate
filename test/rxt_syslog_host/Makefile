CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -I../../include

.PHONY: all test clean

all: rxt_syslog_test

rxt_syslog_test: rxt_syslog_test.cpp ../../include/rxt_syslog.h
	$(CXX) $(CXXFLAGS) $< -o $@

test: rxt_syslog_test
	./rxt_syslog_test

clean:
	rm -f rxt_syslog_test
