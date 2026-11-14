.PHONY: test-digi test-return-route clean-digi-test clean-return-route-test

.PHONY: test-ota-rendezvous
test-ota-rendezvous:
	mkdir -p .pio/host-tests
	$(CXX) -std=c++11 -Wall -Wextra -Werror -Iinclude tools/lora_ota/test_rendezvous.cpp -o .pio/host-tests/ota-rendezvous
	.pio/host-tests/ota-rendezvous

test-digi:
	$(MAKE) -C test/digi-host test

test-return-route:
	$(MAKE) -C test/return_route_host test

clean-digi-test:
	$(MAKE) -C test/digi-host clean

clean-return-route-test:
	$(MAKE) -C test/return_route_host clean
