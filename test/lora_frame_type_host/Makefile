CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -I../../include

.PHONY: test clean
test: frame_type_test
	./frame_type_test

frame_type_test: frame_type_test.cpp ../../include/lora_frame_type.h
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f frame_type_test
