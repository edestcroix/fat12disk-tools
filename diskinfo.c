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
  char *os_name = read_bytes_at(disk, 8, 3);
  printf("OS Name: %s\n", os_name);
}

void print_disk_label(char *buf) {
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

char *str_slice(char *buf, int start, int end) {
  char *result = (char *)malloc((end - start) * sizeof(char));
  strncpy(result, buf + start, end - start);
  return result;
}

int main(int argc, char *argv[]) {
  FILE *disk = fopen(argv[1], "rb");

  char *boot_buf = read_bytes_at(disk, 512, 0);

  char *root_buf = read_bytes_at(disk, 512, 9728);

  // get the os name from the buffer
  printf("OS Name: %s\n", str_slice(boot_buf, 3, 8));
  print_disk_label(root_buf);

  free(root_buf);

  int num_sectors = byte_str_to_int(boot_buf + 19, 2);
  int bytes_per_sector = byte_str_to_int(boot_buf + 11, 2);

  printf("Total size: %d bytes\n", num_sectors * bytes_per_sector);

  int sectors_per_fat = byte_str_to_int(boot_buf + 22, 2);
  int fat_size = sectors_per_fat * bytes_per_sector;
  // the FAT table starts at sector 1, and since each sector
  // is 512 bytes, the FAT table starts at byte 512
  char *fat_table = read_bytes_at(disk, 512 * 9, 512);

  // start with 23 unused sectors.
  int free_sectors = 0;
  // free space is determined by unused sectors * bytes_per_sector.
  // unused sectors are 0x000 in the FAT table. The first
  // two entries in the FAT table are reserved, so we start
  // at the third byte
  fat_table += 3;
  // 33 sectors are reserved, and not part of the FAT table.
  for (int i = 0; i < (num_sectors - 33); i++) {
    uint16_t entry = fat_entry(fat_table, i);
    if (entry == 0x000) {
      free_sectors++;
    }
  }
  fat_table -= 3;

  printf("Free size: %d bytes\n", free_sectors * bytes_per_sector);
  char *root_dir = read_bytes_at(disk, 512 * 14, 9728);
  dir_buf_t root_dir_buf = (dir_buf_t){.buf = root_dir, .size = 14 * 512};
  printf("Number of files: %d\n", count_files(disk, fat_table, &root_dir_buf));

  free(fat_table);
  free(root_dir);
  free(boot_buf);
  printf("FAT copies: %d\n", boot_buf[16]);
  printf("Sectors per FAT: %d\n", sectors_per_fat);
}
