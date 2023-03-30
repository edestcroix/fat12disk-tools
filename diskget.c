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
  directory_t *dirs = root_dirs(disk);

  for (int i = 0; i < DIRS_IN_ROOT; i++) {
    directory_t dir = dirs[i];
    switch (should_skip_dir(dir)) {
    case 1:
      continue;
    case 2:
      goto end;
    default:
      NULL; // My linter complains if I don't have a statement here
      char *filename = filename_ext(dir);
      if (strcmp(filename, target) == 0) {
        printf("Found %s\n", filename);
        uint16_t index = bytes_to_int(dir.first_cluster, 2);

        byte *fat_table = fat_table_buf(disk);
        int size = bytes_to_int(dir.file_size, 4);
        FILE *dest = fopen(filename, "wb");
        copy_file(disk, dest, fat_table, index, size);
        fclose(dest);
        fclose(disk);
        free(dirs);
        printf("File %s copied to current directory.\n", target);
        exit(0);
      }
    }
  }
end:
  printf("%s not found in root directory.\n", target);
}
