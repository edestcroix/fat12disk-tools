/* Prints information about the FAT12 file system on a disk image. Information
 * includes the OS name, disk label, total disk size, FAT table size, free
 * space, number of files, FAT copies, and sectors per FAT. */
#include "fat12.h"

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

  fat12_t fat12 = fat12_from_file(disk);

  // get the os name from the buffer
  printf("OS Name: %s\n", buf_slice(fat12.boot_sector, 3, 8));

  if (fat12.boot_sector[43] != 0x20 && fat12.boot_sector[43] != 0x00) {
    printf("Disk Label: %11.11s\n", fat12.boot_sector + 43);
  } else {
    print_disk_label(disk);
  }

  printf("Total size: %d bytes\n", fat12.total_size);

  printf("FAT size: %d\n", fat12.fat.size);

  printf("Free size: %d bytes\n", fat12.free_space);

  printf("Total number of files: %d\n",
         count_files(disk, fat12.fat.table, fat12.root));
  fclose(disk);

  printf("FAT copies: %d\n", fat12.boot_sector[16]);
  printf("Sectors per FAT: %d\n", bytes_to_ushort(fat12.boot_sector + 22));
  free(fat12.boot_sector);
  free(fat12.fat.table);
}
