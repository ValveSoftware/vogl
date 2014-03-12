include ../Makefile.common

.PHONY: install
install:
	mkdir -p $(prefix)/include/loki
	mkdir -p $(prefix)/include/loki/flex
	mkdir -p $(prefix)/include/loki/yasli
	
	install -m 644 loki/*.h $(prefix)/include/loki
	install -m 644 loki/flex/*.h $(prefix)/include/loki/flex
	install -m 644 loki/yasli/*.h $(prefix)/include/loki/yasli
