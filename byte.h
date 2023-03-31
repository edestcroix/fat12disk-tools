/* Header file for byte.c, which has operations
 * for handling unsigned chars as bytes. And
 * dealing with raw bytes from FAT-12 filesystems. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// I defined a byte type because writing "unsigned char"
// everywhere was getting annoying, and there isn't a uchar
// shorthand line there is for uint and ushort.
typedef unsigned char byte;

// byte sequence conversion functions
uint bytes_to_uint(byte *bytes);
ushort bytes_to_ushort(byte *bytes);
char *bytes_to_filename(byte *bytes);
struct tm bytes_to_time(byte *time_bytes, byte *date_bytes);

// byte sequence read function
byte *read_bytes(FILE *disk, int n, int address);
