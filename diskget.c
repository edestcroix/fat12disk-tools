/* Diskget fetches a file
 * out of the root directory of the disk image
 * into the current directory. (Error
 * if file not found in root dir)
 */

#include "byte.h"
#include "dir_utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  FILE *disk = fopen(argv[1], "rb");
  char *target = argv[2];
  byte *root_dir = root_dir_buf(disk);
  int root_dir_size = 14 * 512;
  directory_t *dirs = get_dir_list(root_dir, root_dir_size / 32);

  int found = 0;
  for (int i = 0; i < root_dir_size / 32; i++) {
    directory_t dir = dirs[i];
    if (dir.filename[0] == 0x00) {
      break;
    }
    char *filename = filename_ext(dir);
    if (strcmp(filename, target) == 0) {
      uint16_t index = bytes_to_int(dir.first_cluster, 2);

      byte *fat_table = fat_table_buf(disk);
      int size = bytes_to_int(dir.file_size, 4);
      unsigned char test_buf[512];

      int test_fat_idx = 0 * 3 / 2;
      strncpy((char *)test_buf, (char *)fat_table + test_fat_idx, 32);

      FILE *dest = fopen(filename, "wb");
      copy_file(disk, dest, fat_table, index, size);
      fclose(dest);
      found = 1;
    }
  }

  if (!found) {
    printf("File not found in root directory\n");
    exit(1);
  }
}
