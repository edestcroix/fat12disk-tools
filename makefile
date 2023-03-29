COMPILER=gcc
CFLAGS=-c -Wall -g 
COMPILE = $(COMPILER) $(CFLAGS)


all: diskinfo disklist diskget

diskget: diskget.c build/dir_utils.o build/byte.o build/fat12.o
	$(COMPILER) $^ -o $@

diskinfo: diskinfo.c build/dir_utils.o build/byte.o build/fat12.o
	$(COMPILER) $^ -o $@

disklist: disklist.c build/dir_utils.o build/byte.o build/fat12.o
	$(COMPILER) $^ -o $@

build/dir_utils.o: dir_utils.c dir_utils.h 
	mkdir -p build
	$(COMPILE) dir_utils.c -o $@

build/byte.o: byte.c byte.h
	mkdir -p build
	$(COMPILE) byte.c -o $@

build/fat12.o: fat12.c fat12.h
	mkdir -p build
	$(COMPILE) fat12.c -o $@

clean: 
	rm -rf build/
	rm -f diskinfo
	rm -f disklist
	rm -f diskget
