#include <stdint.h>
#include <stdio.h>
#include <time.h>

/* Header file for byte.c, which has operations
 * for handling unsigned chars as bytes. And
 * dealing with raw bytes from FAT-12 filesystems.
 */

typedef unsigned char byte;

// byte sequence conversion functions
unsigned int bytes_to_int(byte *bytes, int size);
char *bytes_to_filename(byte *bytes);
struct tm bytes_to_time(byte *bytes);

// byte sequence read functions
byte *read_bytes(FILE *disk, int n, int address);
