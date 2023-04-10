/* Reads the FAT12 filesystem on a floppy disk image and prints
 * the directory structure. Completely ignores all long
 * filenames, and with neither print a long file name,
 * nor print file inside a directory with a long file name. */
#include "fat12.h"

void print_header(char *dirname, char *header_printed) {
  if (!*header_printed) {
    printf("%s\n", dirname);
    printf("===================================================\n");
    *header_printed = 1;
  }
}

/* prints F/D for files/subdirectories, and
 * the file size, (0 for subdirs), the filename,
 * and file creation date. */
void print_dir(directory_t dir) {
  char type = dir.attribute & DIR_MASK ? 'D' : 'F';
  if (type == 'F') {
    struct tm creation_time =
        bytes_to_time(dir.creation_time, dir.creation_date);
    char creation_time_str[20];
    strftime(creation_time_str, 20, "%m/%d/%Y %H:%M:%S", &creation_time);
    printf("%c %-10d%-20s", type, bytes_to_uint(dir.file_size),
           filename_ext(dir));
    printf("%s\n", creation_time_str);
  } else {
    printf("%c %-10s%-20s\n", type, "", filename_ext(dir));
  }
}

void print_dirs(directory_t *dirs, char *dirname, int size) {
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

void parse_dirs(FILE *disk, byte *fat_table, dir_list_t dirs, char *dirname) {
  directory_t *dir_arr = dirs.dirs;
  print_dirs(dir_arr, dirname, dirs.size);
  int n = (strncmp(dirname, "Root", 4)) ? 0 : 1;
  for (int i = 0; i < dirs.size; i++) {
    directory_t dir = dir_arr[i];
    switch (should_skip_dir(dir)) {
    case 1 ... 2:
      continue;
    case 3:
      return;
    default:
      if (dir.attribute & DIR_MASK) {
        ushort index = bytes_to_ushort(dir.first_cluster);
        dir_list_t next_dirs = dir_from_fat(disk, fat_table, index);
        // append the next dir to the current dir
        char *next_dirname = bytes_to_filename(dir.filename);
        strcat(dirname, "/");
        strcat(dirname, next_dirname);
        parse_dirs(disk, fat_table, next_dirs, dirname);
        free(next_dirs.dirs);
      }
    }
  }
  free(dir_arr);
}

int main(int argc, char *argv[]) {
  FILE *disk = open_disk(argv[1], "rb");
  fat12_t fat12 = fat12_from_file(disk);
  char dirname[100] = "Root";
  parse_dirs(disk, fat12.fat.table, fat12.root, dirname);
  free_fat12(fat12);
  fclose(disk);
}
