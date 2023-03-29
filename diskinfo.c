#include "dir_utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 his program is used to get the disk information from
a FAT12 disk image, the name of which is given
as an argument to the program

it needds to retrieve the following:
  - Os name
- Disk label
- Total size
- Free size
- Total number of files
- Number of FAT copies
- Sectors per FAT

*/

void print_os_name(FILE *disk) {
  // the os name is the first 8 bytes of the boot sector
  // the boot sector starts at byte 3
  byte *os_name = read_bytes(disk, 8, 3);
  printf("OS Name: %s\n", os_name);
}

void print_disk_label(byte *buf) {
  // buf is the buffer containing the root directory
  directory_t *dirs = get_dir_list(buf, 16);
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

// converts "length" bytes of a string to an int
int byte_str_to_int(char *str, int length) {
  int result = 0;
  for (int i = 0; i < length; i++) {
    result += str[i] << (8 * i);
  }
  return result;
}

char *buf_slice(byte *buf, int start, int end) {
  char *result = (char *)malloc((end - start) * sizeof(char));
  strncpy(result, (char *)buf + start, end - start);
  return result;
}

int main(int argc, char *argv[]) {
  FILE *disk = fopen(argv[1], "rb");

  byte *boot_buf = boot_sector_buf(disk);
  byte *root_buf = read_bytes(disk, 512, 9728);

  // get the os name from the buffer
  printf("OS Name: %s\n", buf_slice(boot_buf, 3, 8));
  print_disk_label(root_buf);

  free(root_buf);

  int num_sectors = bytes_to_int(boot_buf + 19, 2);
  int bytes_per_sector = bytes_to_int(boot_buf + 11, 2);

  printf("Total size: %d bytes\n", num_sectors * bytes_per_sector);

  int sectors_per_fat = bytes_to_int(boot_buf + 22, 2);
  int fat_size = sectors_per_fat * bytes_per_sector;
  printf("FAT size: %d\n", fat_size);
  // the FAT table starts at sector 1, and since each sector
  // is 512 bytes, the FAT table starts at byte 512
  byte *fat_table = fat_table_buf(disk);

  // start with 23 unused sectors.
  int free_sectors = 0;

  // NOTE: Free space reporting has been (probably) fixed. It's now
  // reporting the same amount of free space as the fatcat thing
  // I found online to test with, but I don't know if that program
  // is correct or not.
  for (int i = 2; i < fat_size * 2 / 3; i++) {
    uint16_t entry = fat_entry(fat_table, i);
    if (entry == 0x000) {
      free_sectors++;
    }
  }
  printf("Free size: %d bytes\n", (free_sectors)*bytes_per_sector);
  byte *root_dir = root_dir_buf(disk);
  dir_buf_t root_dir_buf = (dir_buf_t){.buf = root_dir, .size = 14 * 512};
  printf("Number of files: %d\n", count_files(disk, fat_table, root_dir_buf));

  free(fat_table);
  free(root_dir);
  free(boot_buf);
  printf("FAT copies: %d\n", boot_buf[16]);
  printf("Sectors per FAT: %d\n", sectors_per_fat);
}
