COMPILER=gcc
CFLAGS=-c -Wall -g 
COMPILE = $(COMPILER) $(CFLAGS)
BUILD_DEPS = build/byte.o build/fat12.o


all: diskinfo disklist diskget

diskget: diskget.c $(BUILD_DEPS)
	$(COMPILER) $^ -o $@

diskinfo: diskinfo.c $(BUILD_DEPS)
	$(COMPILER) $^ -o $@

disklist: disklist.c $(BUILD_DEPS)
	$(COMPILER) $^ -o $@

build/byte.o: byte.c byte.h
	mkdir -p build
	$(COMPILE) byte.c -o $@

build/fat12.o: fat12.c fat12.h
	mkdir -p build
	$(COMPILE) fat12.c -o $@

clean: 
	rm -rf build/ diskinfo disklist diskget
