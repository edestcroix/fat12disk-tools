#include "fat12.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void print_dir(directory_t dir, char type, char dirname[9]) {
  printf("%c ", type);
  printf("%-10d", bytes_to_int(dir.file_size, 4));
  // filename is 8 bytes, but not null-terminated.
  char filename[13];
  memset(filename, 0, 12);
  strncpy(filename, bytes_to_filename(dir.filename), 8);
  if (type == 'F') {
    strncat(filename, ".", 2);
    strncat(filename, (char *)dir.extension, 3);
  }
  printf("%-20s", filename);
  struct tm creation_time = bytes_to_time(dir.creation_time);
  char creation_time_str[20];
  strftime(creation_time_str, 20, "%m/%d/%Y %H:%M:%S", &creation_time);
  printf("%s\n", creation_time_str);
}

int print_dirs(directory_t *dirs, char dirname[9]) {
  int i = 0, zero_found = 0, num = 0;
  int header_printed = 0;
  while (!zero_found) {
    directory_t dir = dirs[i++];
    switch (should_skip_dir(dir)) {
    case 1:
      continue;
    case 2:
      goto print_dir_end;
    default:
      if (dir.attribute == DIR_MASK) {
        // only print the header if we have
        // made it to a valid directory
        // that is not . or .. This
        // way, we don't print the header for empty
        // directories.
        if (!header_printed) {
          printf("%s\n", dirname);
          printf("===================================================\n");
          header_printed = 1;
        }
        print_dir(dir, 'D', dirname);
      } else {
        if (!header_printed) {
          printf("%s\n", dirname);
          printf("===================================================\n");
          header_printed = 1;
        }
        print_dir(dir, 'F', dirname);
        num++;
      }
    }
  }
print_dir_end:
  return num;
}

int parse_dirs(FILE *disk, byte *fat_table, dir_list_t dirs, char dirname[9]) {
  int num = 0;
  int num_dirs = dirs.size;
  directory_t *dir_arr = dirs.dirs;
  int i = 0, zero_found = 0;
  num += print_dirs(dir_arr, dirname);
  for (int i = 0; i < num_dirs; i++) {
    directory_t dir = dir_arr[i];

    switch (should_skip_dir(dir)) {
    case 1:
      continue;
    case 2:
      goto parse_dir_end;
    default:
      if (!(dir.attribute & DIR_MASK)) {
        continue;
      }
      uint16_t index = bytes_to_int(dir.first_cluster, 2);
      if (index > 1) {
        char dirname[9];
        strncpy(dirname, bytes_to_filename(dir.filename), 8);
        dir_list_t next_dirs = dir_from_fat(disk, fat_table, index);
        num += parse_dirs(disk, fat_table, next_dirs, dirname);
      }
    }
  }
parse_dir_end:
  return num;
}

int main(int argc, char *argv[]) {
  FILE *disk = fopen(argv[1], "rb");
  byte *fat_table = fat_table_buf(disk);
  directory_t *dirs = root_dirs(disk);
  dir_list_t dir_list = (dir_list_t){.dirs = dirs, .size = DIRS_IN_ROOT};
  parse_dirs(disk, fat_table, dir_list, "Root Dir");
  free(dir_list.dirs);
  fclose(disk);
}
