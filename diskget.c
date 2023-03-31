/* Diskget fetches a file out of the root directory of the disk image
 * into the current directory. (Error if file not found in root dir) */
#include "fat12.h"

int main(int argc, char *argv[]) {
  FILE *disk = fopen(argv[1], "rb");
  char *target = argv[2];
  fat12_t fat12 = fat12_from_file(disk);
  for (int i = 0; i < fat12.root.size; i++) {
    directory_t dir = fat12.root.dirs[i];
    switch (should_skip_dir(dir)) {
    case 1 ... 2:
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
        FILE *dest = fopen(filename, "wb");
        copy_file(disk, dest, fat12.fat.table, index,
                  bytes_to_uint(dir.file_size));
        fclose(dest);
        fclose(disk);
        free_fat12(fat12);
        printf("File %s copied to current directory.\n", target);
        exit(0);
      }
    }
  }
}
