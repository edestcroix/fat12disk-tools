#include "dir_utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// reads n bytes from the file starting at the given address
char *read_bytes_at(FILE *disk, int n, int address) {
  char *buf = (char *)malloc(n * sizeof(char));
  fseek(disk, address, SEEK_SET);
  fread(buf, n, 1, disk);
  return buf;
}

// Reads the directory at the given index in the buffer,
// starting at the given offset. Parses directly into a
// directory_t struct.
directory_t *read_dir_at(char *buf, int offset, int index) {
  directory_t *dir = (directory_t *)malloc(sizeof(directory_t));
  dir = (directory_t *)(buf + offset + index * 32);
  return dir;
}

// function that returns the list of directories in a
// directory buffer, up to limit number of directories
directory_t *get_dir_list(char *buf, int limit) {
  directory_t *dir_list = (directory_t *)malloc(limit * sizeof(directory_t));
  int add_at = 0;
  for (int i = 0; i < limit; i++) {
    // check if the first byte of filename
    // is 0x00, in which case the rest of the
    // directory entries are empty
    directory_t dir = *read_dir_at(buf, 0, i);
    if (dir.filename[0] == 0xE5) {
      continue;
    } else if (dir.filename[0] == 0x00) {
      break;
    } else {
      if (dir.attribute == 0x0F || dir.attribute & 0x08) {
        continue;
      }
      if (dir.attribute & 0x10) {
        if (uint8s_to_int(dir.first_cluster, 2) <= 1) {
        }
      }
      dir_list[add_at++] = dir;
    }
  }
  directory_t *result = (directory_t *)malloc(add_at * sizeof(directory_t));
  for (int i = 0; i < add_at; i++) {
    result[i] = dir_list[i];
  }
  return result;
}

int uint8s_to_int(uint8_t *ls, int size) {
  int result = 0;
  for (int i = 0; i < size; i++) {
    result += ls[i] << (8 * i);
  }
  return result;
}

// each entry in the fat table is 12 bits long, so FAT packing
// so the n'th entry in the FAT table is the low
// 4 bits at 1+(3*n)/2 and hthe 8 bits in position (3*n)/2
// if n is odd, the entry is the high 4 bits at
// (3*n)/2 and the 8 bits in 1+(3*n)/2
uint16_t fat_entry(char *fat_table, int n) {
  char result[2];
  if (n % 2 == 0) {
    result[0] = fat_table[1 + (3 * n) / 2] & 0x0F;
    result[1] = fat_table[(3 * n) / 2];
  } else {
    result[0] = fat_table[(3 * n) / 2] >> 4;
    result[1] = fat_table[(3 * n) / 2 + 1];
  }
  // combined the two bytes into a 12 bit integer
  // AND it with 0xFFF to get rid of the extra bits
  return ((uint8_t)result[1] + ((uint8_t)result[0] << 8)) & 0xFFF;
}

dir_buf_t *dir_buf_from_fat(FILE *disk, char *fat_table, int index) {
  uint16_t next_index = fat_entry(fat_table, index);
  // index is the value of the current sector,
  // next_index is the value of the next sector,
  // found in the fat table at index.

  if (index >= 0xff8) {
    dir_buf_t *buf = malloc(sizeof(dir_buf_t));
    buf->buf = "";
    buf->size = 0;
    return buf;
  }

  // the physical sector number is 33 + FAT entry num -2
  int sector_num = 33 + index - 2;
  char *sector = read_bytes_at(disk, 512, sector_num * 512);

  dir_buf_t *next_buf = dir_buf_from_fat(disk, fat_table, next_index);
  dir_buf_t *buf = malloc(sizeof(dir_buf_t));
  buf->buf = malloc(512 + next_buf->size);
  buf->size = 512 + next_buf->size;
  strncpy(buf->buf, sector, 512);
  strncat(buf->buf, next_buf->buf, next_buf->size);
  free(sector);
  free(next_buf);
  return buf;
}

int is_parent_or_cur(directory_t dir) { return (dir.filename[0] == 0x2E); }

int count_files(FILE *disk, char *fat_table, dir_buf_t *dir_buf) {
  /* works like parse_dir_buf, but doesn't print anything,
   * only counts files */
  int num_dirs = dir_buf->size / 32;
  int num = 0, zero_found = 0, i = 0;
  directory_t *dirs = get_dir_list(dir_buf->buf, num_dirs);
  while (!zero_found) {
    directory_t dir = dirs[i++];
    if (dir.filename[0] == 0x00) {
      zero_found = 1;
    } else if (dir.filename[0] == 0xE5) {
      continue;
    } else if (dir.attribute == 0x10) {
      if (is_parent_or_cur(dir)) {
        continue;
      }
      uint16_t index = uint8s_to_int(dir.first_cluster, 2);
      if (index > 1) {
        dir_buf_t *next_buf = dir_buf_from_fat(disk, fat_table, index);
        num += count_files(disk, fat_table, next_buf);
      }
    } else {
      num++;
    }
  }
  return num;
}
