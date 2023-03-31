/* Diskget fetches a file out of the root directory of the disk image
 * into the current directory. (Error if file not found in root dir) */
#include "fat12.h"

int main(int argc, char *argv[]) {
  FILE *disk = fopen(argv[1], "rb");
  char *target = argv[2];
  directory_t *dirs = root_dirs(disk);

  for (int i = 0; i < DIRS_IN_ROOT; i++) {
    directory_t dir = dirs[i];
    switch (should_skip_dir(dir)) {
    case 2:
      continue;
    case 3:
      printf("%s not found in root directory.\n", target);
      exit(1);
    default:
      NULL; // My linter complains if I don't have a statement here
      char *filename = filename_ext(dir);
      if (strcmp(filename, target) == 0) {
        printf("Found %s\n", filename);
        ushort index = bytes_to_ushort(dir.first_cluster);

        byte *fat_table = fat_table_buf(disk);
        int size = bytes_to_ushort(dir.file_size);
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
}
