#include "fat12.h"

/* prints F/D for files/subdirectories, and
 * the file size, (0 for subdirs), the filename,
 * and file creation date. */
void print_dir(directory_t dir) {
  char type = (dir.attribute & DIR_MASK) ? 'D' : 'F';
  printf("%c ", type);
  printf("%-10d", bytes_to_ushort(dir.file_size));
  // filename is 8 bytes, but not null-terminated.
  char filename[13];
  memset(filename, 0, 12);
  strncpy(filename, bytes_to_filename(dir.filename), 8);
  if (type == 'F') {
    strncat(filename, ".", 2);
    strncat(filename, (char *)dir.extension, 3);
  }
  printf("%-20s", filename);
  struct tm creation_time = bytes_to_time(dir.creation_time, dir.creation_date);
  char creation_time_str[20];
  strftime(creation_time_str, 20, "%m/%d/%Y %H:%M:%S", &creation_time);
  printf("%s\n", creation_time_str);
}

void print_header(char dirname[9], char *header_printed) {
  if (!*header_printed) {
    printf("%s\n", dirname);
    printf("===================================================\n");
    *header_printed = 1;
  }
}

void print_dirs(directory_t *dirs, char dirname[9], int size) {
  char header_printed = 0;
  for (int i = 0; i < size; i++) {
    directory_t dir = dirs[i];
    switch (should_skip_dir(dir)) {
    case 1 ... 2:
      continue;
    case 3:
      return;
    default:
      print_header(dirname, &header_printed);
      print_dir(dir);
    }
  }
}

void parse_dirs(FILE *disk, byte *fat_table, dir_list_t dirs, char dirname[9]) {
  directory_t *dir_arr = dirs.dirs;
  print_dirs(dir_arr, dirname, dirs.size);
  for (int i = 0; i < dirs.size; i++) {
    directory_t dir = dir_arr[i];
    switch (should_skip_dir(dir)) {
    case 1 ... 2:
      continue;
    case 3:
      return;
    default:
      if (!(dir.attribute & DIR_MASK)) {
        continue;
      }
      ushort index = bytes_to_ushort(dir.first_cluster);
      if (index > 1) {
        char dirname[9];
        strncpy(dirname, bytes_to_filename(dir.filename), 8);
        dir_list_t next_dirs = dir_from_fat(disk, fat_table, index);
        parse_dirs(disk, fat_table, next_dirs, dirname);
      }
    }
  }
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
