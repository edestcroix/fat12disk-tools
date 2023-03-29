/* This file contains functions for reading and converting
 * the raw bytes from FAT-12 filesystems into usuable data.
 */
#include "byte.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Converts the little-endian byte sequence
 * provided into the integer value it represents.
 * size is the number of bytes in the sequence. */
unsigned int bytes_to_int(byte *bytes, int size) {
  unsigned int result = 0;
  for (int i = 0; i < size; i++) {
    result += bytes[i] << (8 * i);
  }
  return result;
}

/* Converts a string of bytes into a filename
 * (Really just null-terminates the sequence
 * at the first space and casts to a char pointer) */
// NOTE: this might have issues because
// bytes are unisgned and chars are not,
// but on filenames only this should be fine.
char *bytes_to_filename(byte *bytes) {
  for (int i = 0; i < 8; i++) {
    if (bytes[i] == 0x20) {
      bytes[i] = 0x00;
    }
  }
  return (char *)bytes;
}

// converts the 2 byte times from FAT-12 directories
// into a time struct.
struct tm bytes_to_time(byte *bytes) {
  struct tm time;
  time.tm_year = bytes[0] >> 1;
  time.tm_mon = ((bytes[0] & 0x01) << 3) | (bytes[1] >> 5);
  time.tm_mday = bytes[1] & 0x1F;
  time.tm_hour = bytes[2] >> 3;
  time.tm_min = ((bytes[2] & 0x07) << 3) | (bytes[3] >> 5);
  time.tm_sec = (bytes[3] & 0x1F) * 2;
  return time;
}

// reads n bytes from the file starting at the given address
byte *read_bytes(FILE *disk, int n, int address) {
  byte *buf = (byte *)malloc(n * sizeof(byte));
  fseek(disk, address, SEEK_SET);
  fread(buf, n, 1, disk);
  return buf;
}
