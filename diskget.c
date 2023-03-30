/* Diskget fetches a file
 * out of the root directory of the disk image
 * into the current directory. (Error
 * if file not found in root dir)
 */
#include "fat12.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char *argv[]) {
  FILE *disk = fopen(argv[1], "rb");
  char *target = argv[2];
  int num_dirs = 14 * 512 / 32;
  directory_t *dirs = root_dirs(disk);

  int found = 0;
  for (int i = 0; i < num_dirs; i++) {
    directory_t dir = dirs[i];
    if (dir.filename[0] == 0x00) {
      break;
    }
    char *filename = filename_ext(dir);
    if (strcmp(filename, target) == 0) {
      printf("Found %s\n", filename);
      uint16_t index = bytes_to_int(dir.first_cluster, 2);

      byte *fat_table = fat_table_buf(disk);
      int size = bytes_to_int(dir.file_size, 4);
      FILE *dest = fopen(filename, "wb");
      copy_file(disk, dest, fat_table, index, size);
      fclose(dest);
      found = 1;
    }
  }
  if (!found) {
    printf("%s not found in root directory.\n", target);
    exit(1);
  } else {
    printf("File %s copied to current directory.\n", target);
  }
}
