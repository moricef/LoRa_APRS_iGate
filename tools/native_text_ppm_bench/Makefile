# AC: the AprsCompress source directory, for example
# radiohound/LoRa_APRS_iGate, branch feature/order3-compression, lib/AprsCompress/src
AC ?=
ifeq ($(AC),)
$(error Set AC to the AprsCompress source directory: make AC=/path/to/lib/AprsCompress/src)
endif
ROOT = ../..
CXXFLAGS ?= -std=c++17 -O2
SRC = bench.cpp $(ROOT)/src/native_aprs.cpp $(ROOT)/src/rxt_protocol.cpp \
      $(AC)/AprsCompress.cpp $(AC)/AprsCompressModel.cpp

bench: $(SRC)
	$(CXX) $(CXXFLAGS) -DNATIVE_TEXT_PPM_BENCH -I $(ROOT)/include -I $(AC) $(SRC) -o $@

check: bench
	./bench < vectors.txt > /dev/null

clean:
	rm -f bench

.PHONY: check clean
