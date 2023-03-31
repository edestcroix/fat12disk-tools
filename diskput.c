#include "fat12.h"
#include <assert.h>

/*TODO:
 * - Put filenames into UPPERCASE
 * - Fix timestamps not being set correctly
 *   */

ushort next_free_index(fat_table_t fat, int index) {
  // starting at index, search the fat table for the next free sector,
  for (int i = index + 1; i < fat.valid_sectors; i++) {
    ushort entry = fat_entry(fat.table, i);
    if (entry == 0) {
      return i;
    }
  }
  printf("Error: no free sectors.\n");
  exit(1);
}

void update_fat_table(byte *fat_table, ushort value, int index) {
  // this will suck.

  // the position in the fat table is index *3/2. If index is even,
  // the lower byte of the value goes at this position, and
  // the upper 4 bits are stored in the upper four bits at
  // index *3/2 +1. If index is odd, the upper byte of the value
  // goes at this position, and the lower 4 bits go into the
  // lower 4 bits of the byte at index *3/2 + 1.
  // basically, reverse this:
  // return (n % 2 == 0) ? ((0x00f & b2) << 8) | b1 : b2 << 4 | ((0xf0 & b1) >>
  // 4);

  if (index % 2 == 0) {
    // lower byte goes at index *3/2
    fat_table[3 * index / 2] = (byte)(value & 0x00ff);
    // upper 4 bits go at index *3/2 + 1
    fat_table[3 * index / 2 + 1] &= 0xf0;
    fat_table[3 * index / 2 + 1] |= (byte)((value & 0x0f00) >> 8);
  } else {
    // upper byte goes at index *3/2
    fat_table[3 * index / 2] &= 0x0f;
    fat_table[3 * index / 2] |= (byte)((value & 0x000f) << 4);
    // lower 4 bits go at index *3/2 + 1
    fat_table[3 * index / 2 + 1] = (byte)((value & 0xff0) >> 4);
  }
}

void write_file(FILE *src_file, FILE *dest_disk, fat12_t fat12, int index,
                int size) {
  ushort next_index = next_free_index(fat12.fat, index);
  int sector = index + SECTOR_OFFSET;
  int sector_addr = sector * 512;
  if (size < 512) {
    // last sector of the file.
    char buf[size];
    fread(buf, 1, size, src_file);
    fseek(dest_disk, sector_addr, SEEK_SET);
    fwrite(buf, 1, size, dest_disk);
    update_fat_table(fat12.fat.table, 0xFFF, index);
  } else {
    char *buf = malloc(sizeof(char) * 512);
    fread(buf, 1, 512, src_file);
    fseek(dest_disk, sector_addr, SEEK_SET);
    fwrite(buf, 1, 512, dest_disk);
    free(buf);
    // since write_file is called again after writing,
    // the file pointer for src_file should be moved
    // to the correct place.
    write_file(src_file, dest_disk, fat12, next_index, size - 512);
    update_fat_table(fat12.fat.table, next_index, index);
  }
}

// TODO: This is also trash. And the times are completely wrong.
void add_dir_entry(FILE *disk, dir_list_t dirs, char *filename, uint size,
                   int cluster) {
  directory_t dir;
  directory_t *dir_buf = dirs.dirs;
  int i = 0;
  printf("Filename: %s\n", filename);
  while (i < 13) {
    if (filename[i] == '.') {
      break;
    }
    i++;
  }
  memcpy(dir.filename, filename, i);
  // pad the rest of the filename with 0x20
  memset(dir.filename + i, 0x20, 8 - i);
  memcpy(dir.extension, filename + i + 1, 3);
  dir.attribute = 0x20;
  time_t t = time(NULL);
  struct tm time = *localtime(&t);
  // format the time and date into the dir struct
  ushort time_val =
      (time.tm_sec / 2) | (time.tm_min << 5) | (time.tm_hour << 11);
  ushort date_val =
      (time.tm_mday) | ((time.tm_mon + 1) << 5) | ((time.tm_year - 80) << 9);
  memcpy(dir.last_modified_time, &time_val, 2);
  memcpy(dir.last_modified_date, &date_val, 2);
  // shift the cluster value into little-endian
  for (int i = 0; i < 2; i++) {
    dir.first_cluster[i] = (cluster >> (i * 8)) & 0xff;
  }
  // do the same for size
  for (int i = 0; i < 4; i++) {
    dir.file_size[i] = (size >> (i * 8)) & 0xff;
  }
  printf("Size: %d\n", bytes_to_uint(dir.file_size));

  // interate through the directory buffer and find the first empty entry
  for (int i = 0; i < dirs.size; i++) {
    if (dir_buf[i].filename[0] == 0x00) {
      printf("adding dir entry\n");
      memcpy(&dir_buf[i], &dir, sizeof(directory_t));
      return;
    }
  }
  fseek(disk, 512 * 19, SEEK_SET);
  fwrite(dirs.dirs, sizeof(directory_t), DIRS_IN_ROOT, disk);
}

ushort copy_to_root(FILE *disk, FILE *source, fat12_t fat12, char *filename,
                    int size) {
  // check if the filename already exists
  for (int i = 0; i < DIRS_IN_ROOT; i++) {
    if (strcmp(filename_ext(fat12.root.dirs[i]), filename) == 0) {
      printf("Error: file already exists.\n");
      exit(1);
    }
  }
  ushort free_index = next_free_index(fat12.fat, 2);
  printf("writing to disk\n");
  write_file(source, disk, fat12, free_index, size);
  return free_index;
}

int file_size(FILE *file) {
  fseek(file, 0L, SEEK_END);
  int size = ftell(file);
  fseek(file, 0L, SEEK_SET);
  return size;
}

int main(int argc, char *argv[]) {
  FILE *disk = fopen(argv[1], "rb+");
  if (disk == NULL) {
    printf("Error opening disk image.\n");
  }
  printf("Copying to root_dir\n");
  char *filename = argv[2];
  FILE *source = fopen(filename, "rb");
  if (source == NULL) {
    printf("Error: %s does not exist on host system.\n", filename);
    exit(1);
  }
  fat12_t fat12 = fat12_from_file(disk);

  int size = file_size(source);
  if (size > fat12.free_space) {
    printf("Error: not enough space on disk to store file.\n");
    exit(1);
  }
  // argv[2] is either a file or a directory path,
  // if it's a directory path, the filename is argv[3]/
  if (argc == 4) {
    // TODO: copy to directory
    printf("Copying to directory\n");
    printf("Not implemented yet\n");
    exit(1);
  } else {
    // the file is going to be stored in the root directory
    // check the size of the file
    ushort free_index = copy_to_root(disk, source, fat12, filename, size);

    add_dir_entry(disk, fat12.root, filename, size, free_index);
  }

  // write the updated directory entries to the disk
  // write the FAT table to the disk
  printf("Write Complete\nUpdating FAT table\n");
  fwrite(fat12.fat.table, 1, fat12.fat.size, disk);
  printf("Complete\n");
  free_fat12(fat12);

  return 0;
}
