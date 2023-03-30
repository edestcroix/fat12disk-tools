#include "fat12.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void print_os_name(FILE *disk) {
  // the os name is the first 8 bytes of the boot sector
  // the boot sector starts at byte 3
  byte *os_name = read_bytes(disk, 8, 3);
  printf("OS Name: %s\n", os_name);
}

void print_disk_label(FILE *disk) {
  // buf is the buffer containing the root directory
  directory_t *dirs = root_dirs(disk);
  for (int i = 0; i < DIRS_IN_ROOT; i++) {
    directory_t dir = dirs[i];
    if (dir.filename[0] == 0x00) {
      break;
    }
    // the disk label is the first directory entry
    // with the attribute 0x08
    if (dir.attribute & LABEL_MASK) {
      printf("Disk Label: ");
      for (int j = 0; j < 8; j++) {
        printf("%c", dir.filename[j]);
      }
      printf("\n");
    }
  }
  free(dirs);
}

// converts "length" bytes of a string to an int
int byte_str_to_int(char *str, int length) {
  int result = 0;
  for (int i = 0; i < length; i++) {
    result += str[i] << (8 * i);
  }
  return result;
}

byte *buf_slice(byte *buf, int start, int end) {
  byte *result = (byte *)malloc((end - start) * sizeof(byte));
  memcpy(result, buf + start, end - start);
  return result;
}

int main(int argc, char *argv[]) {
  FILE *disk = fopen(argv[1], "rb");

  byte *boot_buf = boot_sector_buf(disk);

  // get the os name from the buffer
  printf("OS Name: %s\n", buf_slice(boot_buf, 3, 8));
  print_disk_label(disk);

  int num_sectors = bytes_to_int(boot_buf + 19, 2);
  int bytes_per_sector = bytes_to_int(boot_buf + 11, 2);

  printf("Total size: %d bytes\n", num_sectors * bytes_per_sector);

  int sectors_per_fat = bytes_to_int(boot_buf + 22, 2);
  int fat_size = sectors_per_fat * bytes_per_sector;
  printf("FAT size: %d\n", fat_size);

  byte *fat_table = fat_table_buf(disk);

  int free_sectors = 0;

  // NOTE: Free space reporting has been (probably) fixed. It's now
  // reporting the same amount of free space as the fatcat thing
  // I found online to test with, but I don't know if that program
  // is correct or not.
  for (int i = 0; i < fat_size * 2 / 3; i++) {
    uint16_t entry = fat_entry(fat_table, i);
    if (entry == 0x000) {
      free_sectors++;
    }
  }
  printf("Free size: %d bytes\n", (free_sectors)*bytes_per_sector);

  directory_t *dirs = root_dirs(disk);
  dir_list_t dir_list = (dir_list_t){.dirs = dirs, .size = DIRS_IN_ROOT};
  printf("Total number of files: %d\n", count_files(disk, fat_table, dir_list));

  free(fat_table);
  free(boot_buf);
  fclose(disk);
  printf("FAT copies: %d\n", boot_buf[16]);
  printf("Sectors per FAT: %d\n", sectors_per_fat);
}
