CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -I../../include

.PHONY: test
test:
	$(CXX) $(CXXFLAGS) report_test.cpp -o /tmp/igate-telemetry-report-test
	/tmp/igate-telemetry-report-test
