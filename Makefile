release:
	cd MonaBase && $(MAKE) && cd ../MonaCore && $(MAKE) && cd ../MonaTiny && $(MAKE) && cd ../MonaServer && $(MAKE) && cd ../UnitTests && $(MAKE)

debug:
	cd MonaBase && $(MAKE) debug && cd ../MonaCore && $(MAKE) debug && cd ../MonaTiny && $(MAKE) debug && cd ../MonaServer && $(MAKE) debug &&cd ../UnitTests && $(MAKE) debug

clean:
	cd MonaBase && $(MAKE) clean && cd ../MonaCore && $(MAKE) clean && cd ../MonaTiny && $(MAKE) clean && cd ../MonaServer && $(MAKE) clean &&cd ../UnitTests && $(MAKE) clean

