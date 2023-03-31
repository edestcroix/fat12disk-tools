#include "fat12.h"
#include <assert.h>

/*TODO:
 * - Put filenames into UPPERCASE
 * - Fix timestamps not being set correctly
 *   */

ushort next_free_index(byte *fat_table, int index) {
  // starting at index, search the fat table for the next free sector,
  for (int i = index + 1; i < 2848; i++) {
    ushort entry = fat_entry(fat_table, i);
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

  printf("Old bytes: %x, %x\n", fat_table[index * 3 / 2],
         fat_table[index * 3 / 2 + 1]);

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
  printf("New bytes: %x, %x\n", fat_table[index * 3 / 2],
         fat_table[index * 3 / 2 + 1]);
}

void write_file(FILE *src_file, FILE *dest_disk, byte *fat_table, int index,
                int size) {
  ushort next_index = next_free_index(fat_table, index);
  printf("next index: %d\n", next_index);
  int sector = index + SECTOR_OFFSET;
  int sector_addr = sector * 512;
  printf("sector: %d\n", sector);
  printf("size_left: %d\n", size);
  if (size < 512) {
    // last sector of the file.
    printf("at last sector of file.\n");
    printf("writing to sector: %d\n", sector);
    char buf[size];
    fread(buf, 1, size, src_file);
    fseek(dest_disk, sector_addr, SEEK_SET);
    fwrite(buf, 1, size, dest_disk);
    update_fat_table(fat_table, 0xFFF, index);
  } else {
    char *buf = malloc(sizeof(char) * 512);
    fread(buf, 1, 512, src_file);
    printf("writing to sector: %d\n", sector);
    fseek(dest_disk, sector_addr, SEEK_SET);
    fwrite(buf, 1, 512, dest_disk);
    free(buf);
    // since write_file is called again after writing,
    // the file pointer for src_file should be moved
    // to the correct place.
    write_file(src_file, dest_disk, fat_table, next_index, size - 512);
    update_fat_table(fat_table, next_index, index);
  }
}

void add_dir_entry(dir_list_t dirs, char *filename, uint size, int cluster) {
  directory_t dir;
  directory_t *dir_buf = dirs.dirs;
  int i = 0;
  printf("filename: %s\n", filename);
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
  printf("Times\n");
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
  printf("filesize %d\n", size);
  printf("size: %d\n", bytes_to_uint(dir.file_size));

  // interate through the directory buffer and find the first empty entry
  for (int i = 0; i < dirs.size; i++) {
    if (dir_buf[i].filename[0] == 0x00) {
      printf("adding dir entry\n");
      memcpy(&dir_buf[i], &dir, sizeof(directory_t));
      return;
    }
  }
}

// TODO: This code looks like garbage.
int main(int argc, char *argv[]) {
  FILE *disk = fopen(argv[1], "rb+");
  if (disk == NULL) {
    printf("Error opening disk image.\n");
  }
  // argv[2] is either a file or a directory path,
  // if it's a directory path, the filename is argv[3]/
  if (argc == 4) {
    char *dir_path = argv[2];
    // split the path into a list of directories
    char *dirs = malloc(sizeof(char) * 9 * 16);
    char *split = strtok(dir_path, "/");
    for (int i = 0; split != NULL; i++) {
      split = strtok(NULL, "/");
      dirs[i] = *split;
    }
    printf("not working yet\n");
    exit(1);
    char *filename = argv[3];
  } else {
    printf("Copying to root_dir\n");
    char *filename = argv[2];
    FILE *source = fopen(filename, "rb");
    if (source == NULL) {
      printf("Error: %s does not exist on host system.\n", filename);
      exit(1);
    }
    directory_t *dirs = root_dirs(disk);
    // check if the filename already exists
    for (int i = 0; i < DIRS_IN_ROOT; i++) {
      if (strcmp(filename_ext(dirs[i]), filename) == 0) {
        printf("Error: file already exists.\n");
        exit(1);
      }
    }
    // the file is going to be stored in the root directory
    byte *boot_buf = boot_sector_buf(disk);
    int num_sectors = bytes_to_ushort(boot_buf + 19);
    byte *fat_table = fat_table_buf(disk);
    int dest_space = free_space(fat_table, num_sectors);
    // check the size of the file
    int source_size = 0;
    fseek(source, 0L, SEEK_END);
    source_size = ftell(source);
    fseek(source, 0L, SEEK_SET);
    if (source_size > dest_space) {
      printf("Error: not enough space on disk to store file.\n");
      exit(1);
    }
    ushort free_index = next_free_index(fat_table, 2);
    printf("free index: %d\n", free_index);
    write_file(source, disk, fat_table, free_index, source_size);
    dir_list_t dir_list = {.dirs = dirs, .size = DIRS_IN_ROOT};
    printf("Write Complete\n");
    add_dir_entry(dir_list, filename, source_size, free_index);
    // write the updated directory entries to the disk
    fseek(disk, 512 * 19, SEEK_SET);
    printf("writing to disk\n");
    fwrite(dirs, sizeof(directory_t), DIRS_IN_ROOT, disk);
    // write the FAT table to the disk
    byte *boot_sector = boot_sector_buf(disk);
    int fat_size = boot_sector[22] + (boot_sector[23] << 8);
    int reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);
    int fat_start = reserved_sectors;
    int fat_size_bytes = fat_size * SECTOR_SIZE;
    printf("fat_size: %d\n", fat_size_bytes);
    fseek(disk, fat_start * SECTOR_SIZE, SEEK_SET);
    fwrite(fat_table, 1, fat_size_bytes, disk);
  }

  return 0;
}
