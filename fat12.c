#include "byte.h"
#include "dir_utils.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int FAT_START = 0x0000200;

// if n is even, the first byte of the entry is in the
// (3*n/2)th byte of the table, and then the lower 4 bits
// in the 1 + (3*n/2)th byte must be prependend to
// the first byte.
// If n is odd, the first byte of the entry is in the
// 1 + (3*n/2)th byte of the table, and then the upper 4 bits
// of the (3*n/2)th byte must be prepended to the first byte.
uint16_t fat_entry(byte *fat_table, int n) {
  // first, read the 2 bytes starting at
  // (3*n/2)th byte of the table
  unsigned char entry[2];
  // NOTE: VERY Important that MEMCPY is used and
  // not strncpy, because strncpy will stop at,
  // null bytes, which we don't want.
  memcpy(entry, fat_table + ((3 * n) / 2), 2);
  if (n % 2 == 0) {
    // if n is even, the first byte of the entry is
    // in position 0, and we have to get the low 4
    // bytes of the second byte, shift it left
    // 8 bits, and OR it with the first byte.
    uint16_t first_byte = entry[0];
    uint16_t second_byte = entry[1];
    return ((0x00f & second_byte) << 8) | first_byte;
  } else {
    uint16_t first_byte = entry[0];
    uint16_t second_byte = entry[1];
    return second_byte << 4 | ((0xf0 & first_byte) >> 4);
  }
}

// starting at the index'th entry in the FAT table,
// copy the sectors in the FAT chain to the output file.
void copy_file(FILE *src_disk, FILE *out, byte *fat_table, int index,
               int size) {
  uint16_t next_index = fat_entry(fat_table, index);
  int sector_num = 33 + index - 2;

  if (next_index >= 0xff8 || next_index == 0x00) {
    // this is the end of the chain,
    // read the sector and return it
    byte *sector = read_bytes(src_disk, 512, sector_num * 512);
    fwrite(sector, size, 1, out);
    free(sector);
    return;
  }
  byte *sector = read_bytes(src_disk, 512, sector_num * 512);
  fwrite(sector, 512, 1, out);
  copy_file(src_disk, out, fat_table, next_index, size - 512);
}

byte *boot_sector_buf(FILE *disk) {
  fseek(disk, 0, SEEK_SET);
  byte *buf = (byte *)malloc(512 * sizeof(byte));
  fread(buf, 512, 1, disk);
  return buf;
}

byte *fat_table_buf(FILE *disk) {
  byte *boot_sector = boot_sector_buf(disk);
  int fat_size = boot_sector[22] + (boot_sector[23] << 8);
  int reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);
  int fat_start = reserved_sectors;
  int fat_size_bytes = fat_size * 512;
  byte *fat_table = (byte *)malloc(fat_size_bytes * sizeof(byte));
  fseek(disk, 512 * fat_start, SEEK_SET);
  fread(fat_table, fat_size_bytes, 1, disk);
  return fat_table;
}

byte *root_dir_buf(FILE *disk) {
  fseek(disk, 9728, SEEK_SET);
  byte *buf = (byte *)malloc(14 * 512 * sizeof(byte));
  fread(buf, 14 * 512, 1, disk);
  return buf;
}

char *filename_ext(directory_t dir) {
  char *filename = (char *)malloc(13 * sizeof(char));
  memset(filename, 0, 12);
  strncpy(filename, bytes_to_filename(dir.filename), 8);
  strncat(filename, ".", 2);
  strncat(filename, (char *)dir.extension, 3);
  return filename;
}
