COMPILER=gcc
CFLAGS=-c -Wall -g 
COMPILE = $(COMPILER) $(CFLAGS)


all: diskinfo disklist

diskinfo: diskinfo.c build/dir_utils.o build/byte.o
	$(COMPILER) $< build/dir_utils.o build/byte.o -o $@

disklist: disklist.c build/dir_utils.o build/byte.o
	$(COMPILER) $< build/dir_utils.o build/byte.o -o $@

build/dir_utils.o: dir_utils.c dir_utils.h 
	mkdir -p build
	$(COMPILE) dir_utils.c -o $@

build/byte.o: byte.c byte.h
	mkdir -p build
	$(COMPILE) byte.c -o $@

clean: 
	rm -rf build/
	rm -f diskinfo
	rm -f disklist
