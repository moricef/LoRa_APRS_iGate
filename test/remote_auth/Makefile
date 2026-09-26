CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror
TEST_BIN := ../../.pio/build/remote_auth/replay_window_test

.PHONY: test clean

test: $(TEST_BIN)
	$(TEST_BIN)
	python3 test_vectors.py

$(TEST_BIN): replay_window_test.cpp ../../include/remote_replay_window.h
	mkdir -p $(dir $(TEST_BIN))
	$(CXX) $(CXXFLAGS) -I../../include replay_window_test.cpp -o $@

clean:
	rm -f $(TEST_BIN)
