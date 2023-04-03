/* This file contains functions for reading and converting
 * the raw bytes from FAT-12 filesystems into usuable data.
 */
#include "byte.h"

/* Converts the little-endian byte sequence
 * provided into the integer value it represents.
 * size is the number of bytes in the sequence. */
uint bytes_to_uint(byte *bytes) {
  unsigned int result = 0;
  for (int i = 0; i < 4; i++) {
    result += bytes[i] << (8 * i);
  }
  return result;
}

ushort bytes_to_ushort(byte *bytes) {
  ushort result = 0;
  for (int i = 0; i < 2; i++) {
    result += bytes[i] << (8 * i);
  }
  return result;
}

/* Converts a string of bytes into a filename
 * (Really just null-terminates the sequence
 * at the first space and casts to a char pointer) */
char *bytes_to_filename(byte *bytes) {
  for (int i = 0; i < 8; i++) {
    if (bytes[i] == 0x20) {
      bytes[i] = 0x00;
    }
  }
  return (char *)bytes;
}

// converts the times and dates from FAT-12 directories into a time struct.
struct tm bytes_to_time(byte *time_bytes, byte *date_bytes) {
  // some funky pointer casting because we need the extra space
  // of a ushort for bit shifting without overflow.
  ushort times = *(ushort *)time_bytes, dates = *(ushort *)date_bytes;
  struct tm time;
  // FAT uses a different year offset.
  time.tm_year = (dates >> 9) + 80;
  // it also uses 1-12 for months, not 0-11.
  time.tm_mon = ((dates >> 5) & 0x0F) - 1;
  time.tm_mday = dates & 0x1F;
  time.tm_hour = times >> 11;
  time.tm_min = (times >> 5) & 0x3F;
  // seconds are stored in 2-second increments.
  time.tm_sec = (times & 0x1F) * 2;
  return time;
}

// reads n bytes from the file starting at the given address
byte *read_bytes(FILE *disk, int n, int address) {
  byte *buf = (byte *)malloc(n * sizeof(byte));
  fseek(disk, address, SEEK_SET);
  if (fread(buf, n, 1, disk) < 1) {
    printf("Error reading bytes from disk.\n");
    exit(1);
  };
  return buf;
}
