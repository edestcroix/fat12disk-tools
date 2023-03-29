#include "dir_utils.h"
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
  char creation_time_str[20];
  struct tm creation_time = bytes_to_time(dir.creation_time);
  strftime(creation_time_str, 20, "%m/%d/%Y %H:%M:%S", &creation_time);
  printf("%s\n", creation_time_str);
}

int print_dirs(directory_t *dirs, char dirname[9]) {
  int i = 0, zero_found = 0, num = 0;
  int header_printed = 0;
  while (!zero_found) {
    directory_t dir = dirs[i++];
    // print the attribute as a hex value
    // padded to 2 digits
    if (dir.filename[0] == 0x00) {
      // the only way to tell if we have
      // reached the end of the directory
      // list is if we hit a 0x00,
      // because the directory list is not
      // null-terminated. If we get 0x00 here,
      // it means that get_dir_list has
      // stopped adding to the list here.
      zero_found = 1;
    } else if (dir.filename[0] == 0xE5) {
      continue;
    } else if (dir.attribute == 0x10) {
      // check if the dir is . or ..
      if (is_parent_or_cur(dir)) {
        continue;
      }
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
  return num;
}

int parse_dir_buf(FILE *disk, byte *fat_table, dir_buf_t *dir_buf,
                  char dirname[9]) {
  // get all the subdirectories of the buf,
  // if they are files, print their info,
  // if they are directories, recurse

  int num = 0;
  int num_dirs = dir_buf->size / 32;
  directory_t *dirs = get_dir_list(dir_buf->buf, num_dirs);
  int i = 0, zero_found = 0;
  num += print_dirs(dirs, dirname);
  // go back through the dirs list
  // and recurse on any directories
  for (int i = 0; i < num_dirs; i++) {
    directory_t dir = dirs[i];
    if (dir.filename[0] == 0x00) {
      break;
    }

    if (!(dir.attribute & 0x10)) {
      continue;
    }
    if (is_parent_or_cur(dir)) {
      continue;
    }
    uint16_t index = bytes_to_int(dir.first_cluster, 2);
    if (index > 1) {
      dir_buf_t *next_buf = dir_buf_from_fat(disk, fat_table, index);
      char dirname[9];
      strncpy(dirname, bytes_to_filename(dir.filename), 8);
      num += parse_dir_buf(disk, fat_table, next_buf, dirname);
    }
  }
  free(dirs);
  return num;
}

int main(int argc, char *argv[]) {
  FILE *disk = fopen(argv[1], "rb");
  int root_dir_size = 14 * 512;
  byte *root_dir = read_bytes(disk, 512 * 14, 9728);
  byte *fat_table = read_bytes(disk, 512 * 9, 512);
  dir_buf_t root_buf = (dir_buf_t){.buf = root_dir, .size = root_dir_size};
  parse_dir_buf(disk, fat_table, &root_buf, "Root Dir");
}
