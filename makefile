COMPILER=gcc
CFLAGS=-c -Wall -g 
COMPILE = $(COMPILER) $(CFLAGS)


all: diskinfo disklist

diskinfo: diskinfo.c build/dir_utils.o
	$(COMPILER) $< build/dir_utils.o -o diskinfo

disklist: disklist.c build/dir_utils.o
	$(COMPILER) $< build/dir_utils.o -o disklist

build/dir_utils.o: dir_utils.c dir_utils.h
	mkdir -p build
	$(COMPILE) dir_utils.c -o $@

clean: 
	rm -rf build/
	rm -f diskinfo
