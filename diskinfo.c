/* Prints information about the FAT12 file system on a disk image. Information
 * includes the OS name, disk label, total disk size, FAT table size, free
 * space, number of files, FAT copies, and sectors per FAT. */
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
    printf("Disk Label: %8.8s\n", dir.filename);
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

  if (boot_buf[43] != 0x20 && boot_buf[43] != 0x00) {
    printf("Disk Label: %11.11s\n", boot_buf + 43);
  } else {
    print_disk_label(disk);
  }

  ushort num_sectors = bytes_to_ushort(boot_buf + 19);
  ushort bytes_per_sector = bytes_to_ushort(boot_buf + 11);
  free(boot_buf);

  printf("Total size: %d bytes\n", num_sectors * bytes_per_sector);

  ushort sectors_per_fat = bytes_to_ushort(boot_buf + 22);
  int fat_size = sectors_per_fat * bytes_per_sector;
  printf("FAT size: %d\n", fat_size);
  byte *fat_table = fat_table_buf(disk);

  printf("Free size: %d bytes\n", free_space(fat_table, num_sectors));

  directory_t *dirs = root_dirs(disk);
  dir_list_t dir_list = (dir_list_t){.dirs = dirs, .size = DIRS_IN_ROOT};
  printf("Total number of files: %d\n", count_files(disk, fat_table, dir_list));

  free(fat_table);
  fclose(disk);
  printf("FAT copies: %d\n", boot_buf[16]);
  printf("Sectors per FAT: %d\n", sectors_per_fat);
}
