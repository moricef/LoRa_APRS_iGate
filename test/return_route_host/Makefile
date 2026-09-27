CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -I../../include

.PHONY: test clean

return_route_test: return_route_test.cpp ../../src/aprs_return_route.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

test: return_route_test
	./return_route_test

clean:
	rm -f return_route_test
