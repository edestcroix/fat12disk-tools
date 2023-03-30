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
  directory_t *dirs = sector_dirs(disk, 19);
  for (int i = 0; i < 16; i++) {
    directory_t dir = dirs[i];
    // the disk label is the first directory entry
    // with the attribute 0x08
    if (dir.attribute == 0x08) {
      printf("Disk Label: ");
      for (int j = 0; j < 8; j++) {
        printf("%c", dir.filename[j]);
      }
      printf("\n");
    }
  }
  free(dirs);
}

char *buf_slice(byte *buf, int start, int end) {
  char *result = (char *)malloc((end - start) * sizeof(char));
  strncpy(result, (char *)buf + start, end - start);
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

  // NOTE: Free space reporting is now the same
  // as the WinImage tool that created the images,
  // so I consider that correct.
  for (int i = 2; i < 2848; i++) {
    uint16_t entry = fat_entry(fat_table, i);
    if (entry == 0x000) {
      free_sectors++;
    }
  }
  printf("Free size: %d bytes\n", (free_sectors)*bytes_per_sector);

  int root_dir_size = 14 * 512, dirs_in_root = root_dir_size / 32;
  directory_t *dirs = root_dirs(disk);
  dir_list_t dir_list = (dir_list_t){.dirs = dirs, .size = dirs_in_root};
  printf("Total number of files: %d\n", count_files(disk, fat_table, dir_list));

  free(fat_table);
  free(boot_buf);
  fclose(disk);
  printf("FAT copies: %d\n", boot_buf[16]);
  printf("Sectors per FAT: %d\n", sectors_per_fat);
}
