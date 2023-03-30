/* Header file for byte.c, which has operations
 * for handling unsigned chars as bytes. And
 * dealing with raw bytes from FAT-12 filesystems. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef unsigned char byte;

// byte sequence conversion functions
ushort bytes_to_ushort(byte *bytes);
char *bytes_to_filename(byte *bytes);
struct tm bytes_to_time(byte *time_bytes, byte *date_bytes);

// byte sequence read functions
byte *read_bytes(FILE *disk, int n, int address);
