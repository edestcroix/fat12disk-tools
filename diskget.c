/* Diskget fetches a file out of the root directory of the disk image
 * into the current directory. (Error if file not found in root dir) */
#include "fat12.h"
#include <ctype.h>

/* Copies the file starting at the sector in the FAT-12
 * filesystem in src_disk corresponding to index into the out file
 * on the host filesystem. */
void copy_file(FILE *src, FILE *out, byte *fat_table, int index, int size) {
  ushort next_index = fat_entry(fat_table, index);
  byte *sector = read_sector(src, index + SECTOR_OFFSET);

  if (last_sector(next_index, "copy_file")) {
    fwrite(sector, size, 1, out);
  } else {
    fwrite(sector, SECTOR_SIZE, 1, out);
    copy_file(src, out, fat_table, next_index, size - SECTOR_SIZE);
  }
  free(sector);
}

int main(int argc, char *argv[]) {
  FILE *disk = fopen(argv[1], "rb");
  char *target = argv[2];
  for (int i = 0; target[i]; i++) {
    target[i] = toupper(target[i]);
  }
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
  printf("%s not found in root directory.\n", target);
  fclose(disk);
  free_fat12(fat12);
  exit(1);
}
