#include "dir_utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* since the filename is not null-terminated,
 * need to convert it to a null-terminated string,
 * by finding the first 0x20 and replacing it with 0x00.
 * (0x20 being the space character)
 */
char *filename_to_chars(uint8_t *filename) {
  for (int i = 0; i < 8; i++) {
    if (filename[i] == 0x20) {
      filename[i] = 0x00;
    }
  }
  return (char *)filename;
}

int parse_dir_buf(FILE *disk, char *fat_table, dir_buf_t *dir_buf,
                  char dirname[9]) {
  // get all the subdirectories of the buf,
  // if they are files, print their info,
  // if they are directories, recurse

  printf("%s\n", dirname);
  printf("================\n");
  int num = 0;
  int num_dirs = dir_buf->size / 32;
  directory_t *dirs = get_dir_list(dir_buf->buf, num_dirs);
  int i = 0, zero_found = 0;
  while (!zero_found) {
    directory_t dir = dirs[i++];
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

      printf("D ");
      char dirname[9];
      memset(dirname, 0, 9);
      strncpy(dirname, filename_to_chars(dir.filename), 8);
      printf("%-20s\n", dirname);
    } else {
      printf("F ");
      printf("%-10d", uint8s_to_int(dir.file_size, 4));
      // filename is 8 bytes, but not null-terminated.
      char filename[13];
      memset(filename, 0, 12);
      strncpy(filename, filename_to_chars(dir.filename), 8);
      strncat(filename, ".", 2);
      strncat(filename, (char *)dir.extension, 3);
      printf("%-20s\n", filename);
      num++;
    }
  }
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
    uint16_t index = uint8s_to_int(dir.first_cluster, 2);
    if (index > 1) {
      dir_buf_t *next_buf = dir_buf_from_fat(disk, fat_table, index);
      char dirname[9];
      strncpy(dirname, filename_to_chars(dir.filename), 8);
      num += parse_dir_buf(disk, fat_table, next_buf, dirname);
    }
  }
  free(dirs);
  return num;
}

int main(int argc, char *argv[]) {
  FILE *disk = fopen(argv[1], "rb");
  int root_dir_size = 14 * 512;
  char *root_dir = read_bytes_at(disk, 512 * 14, 9728);
  char *fat_table = read_bytes_at(disk, 512 * 9, 512);
  dir_buf_t root_buf = (dir_buf_t){.buf = root_dir, .size = root_dir_size};
  parse_dir_buf(disk, fat_table, &root_buf, "Root Dir");

  int num_files = count_files(disk, fat_table, &root_buf);
  printf("Total files: %d\n", num_files);
}
