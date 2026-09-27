.PHONY: test-digi test-return-route clean-digi-test clean-return-route-test

test-digi:
	$(MAKE) -C test/digi-host test

test-return-route:
	$(MAKE) -C test/return_route_host test

clean-digi-test:
	$(MAKE) -C test/digi-host clean

clean-return-route-test:
	$(MAKE) -C test/return_route_host clean
