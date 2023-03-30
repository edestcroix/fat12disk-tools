#include "fat12.h"

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
    int skip = should_skip_dir(dir);
    if (skip == 3) {
      break;
    } else if (skip != 1) {
      continue;
    }
    // the disk label is the first directory entry
    // with the attribute 0x08
    if (dir.attribute == LABEL_MASK) {
      printf("Disk Label: ");
      for (int j = 0; j < 8; j++) {
        printf("%c", dir.filename[j]);
      }
      printf("\n");
    }
  }
  free(dirs);
}

byte *buf_slice(byte *buf, int start, int end) {
  byte *result = malloc((end - start) * sizeof(char));
  memcpy(result, buf + start, end - start);
  return result;
}

int main(int argc, char *argv[]) {
  FILE *disk = fopen(argv[1], "rb");

  byte *boot_buf = boot_sector_buf(disk);

  // get the os name from the buffer
  printf("OS Name: %s\n", buf_slice(boot_buf, 3, 8));
  print_disk_label(disk);

  ushort num_sectors = bytes_to_ushort(boot_buf + 19);
  printf("Num sectors: %d\n", num_sectors);
  ushort bytes_per_sector = bytes_to_ushort(boot_buf + 11);

  printf("Total size: %d bytes\n", num_sectors * bytes_per_sector);

  ushort sectors_per_fat = bytes_to_ushort(boot_buf + 22);
  int fat_size = sectors_per_fat * bytes_per_sector;
  printf("FAT size: %d\n", fat_size);
  byte *fat_table = fat_table_buf(disk);

  int free_sectors = 0;
  // the first 2 entries in the fat table are reserved,
  // and there are 32 sectors that are not available for
  // data storage which should be excluded.
  for (int i = 2; i < num_sectors - 32; i++) {
    free_sectors += (fat_entry(fat_table, i) == 0x000);
  }
  printf("Free size: %d bytes\n", free_sectors * bytes_per_sector);

  directory_t *dirs = root_dirs(disk);
  dir_list_t dir_list = (dir_list_t){.dirs = dirs, .size = DIRS_IN_ROOT};
  printf("Total number of files: %d\n", count_files(disk, fat_table, dir_list));

  free(fat_table);
  free(boot_buf);
  fclose(disk);
  printf("FAT copies: %d\n", boot_buf[16]);
  printf("Sectors per FAT: %d\n", sectors_per_fat);
}
