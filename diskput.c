#include "fat12.h"
#include <assert.h>
#include <ctype.h>

/*TODO:
 * - Fix timestamps not being set correctly
 * - Move some functions to fat12.c (Maybe. Actually, probably not because no
 *   other files need any of these functions.)
 *   */

ushort next_free_index(fat_table_t fat, int index) {
  // starting at index+1, search the fat table for the next free sector,
  // because the current index might not be updated yet.
  for (int i = index + 1; i < fat.valid_sectors; i++) {
    ushort entry = fat_entry(fat.table, i);
    if (entry == 0) {
      return i;
    }
  }
  printf("Error: no free sectors.\n");
  exit(1);
}

/* Updates the FAT table value at index to the value given. Operates on the FAT
 * table as a buffer, not the FAT table on the disk, so that if the program
 * fails before finishing the write, the FAT table isn't updated, and the
 * unfinished sectors aren't marked as used. */
void update_fat_table(byte *fat_table, ushort value, int index) {
  if (index % 2 == 0) {
    fat_table[3 * index / 2] = (byte)(value & 0x00ff);
    fat_table[3 * index / 2 + 1] &= 0xf0;
    fat_table[3 * index / 2 + 1] |= (byte)((value & 0x0f00) >> 8);
  } else {
    fat_table[3 * index / 2] &= 0x0f;
    fat_table[3 * index / 2] |= (byte)((value & 0x000f) << 4);
    fat_table[3 * index / 2 + 1] = (byte)((value & 0xff0) >> 4);
  }
}

/* Recursivly writes the file to the disk until the remaining file size is less
 * than a sector, updates the FAT table buffer along the way. */
void write_file(FILE *src_file, FILE *dest_disk, fat12_t fat12, int index,
                int size) {
  int sector = (index + SECTOR_OFFSET) * SECTOR_SIZE;
  if (size < 512) {
    char buf[size];
    fread(buf, 1, size, src_file);
    fseek(dest_disk, sector, SEEK_SET);
    fwrite(buf, 1, size, dest_disk);
    update_fat_table(fat12.fat.table, 0xFFF, index);
  } else {
    char buf[SECTOR_SIZE];
    fread(buf, 1, SECTOR_SIZE, src_file);
    fseek(dest_disk, sector, SEEK_SET);
    fwrite(buf, 1, SECTOR_SIZE, dest_disk);
    ushort next_index = next_free_index(fat12.fat, index);
    write_file(src_file, dest_disk, fat12, next_index, size - SECTOR_SIZE);
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
  // FIXME: This is trash. The times are completely incorrect.
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
      printf("Adding Directory Entry\n");
      memcpy(&dir_buf[i], &dir, sizeof(directory_t));
      return;
    }
  }
  // NOTE: If there is no room for a new directory entry,
  // make sure to fail and exit immediatly, so that
  // the FAT table is not updated. This way, the
  // sector that was supposed to be written to will
  // get overwritten later; as long as the fat table
  // is not updated, and there isn't a directory entry,
  // the copied file effectively doesn't exist.
  //
  // In adding a new directory, if the current sector is full, will have to
  // update the FAT table. Will also have to iterate through the subdirectories
  // sectors to find the first available one until there is no more space,
  // in which case allocate more.
}

void copy_to_root(FILE *disk, FILE *source, fat12_t fat12, char *filename,
                  int size) {
  // check if the filename already exists
  for (int i = 0; i < DIRS_IN_ROOT; i++) {
    if (strcmp(filename_ext(fat12.root.dirs[i]), filename) == 0) {
      printf("Error: file already exists.\n");
      exit(1);
    }
  }
  ushort free_index = next_free_index(fat12.fat, 2);
  printf("Writing to Disk\n");
  write_file(source, disk, fat12, free_index, size);

  add_dir_entry(disk, fat12.root, filename, size, free_index);
  // write the updated directory entries to the disk
  // write the FAT table to the disk
  printf("Write Complete\nUpdating FAT Table\n");
  fseek(disk, 512 * 1, SEEK_SET);
  fwrite(fat12.fat.table, fat12.fat.size, 1, disk);
  fseek(disk, 512 * 19, SEEK_SET);
  fwrite(fat12.root.dirs, sizeof(directory_t), DIRS_IN_ROOT, disk);
  printf("Complete\n");
}

/* TODO: Writing to a subdirectory, this will be similar
 * to copy_to_root, in that the file can just be copied
 * in at the first available sector, but will have to
 * write the directory entry to the subdirectory, not
 * the root.
 * */

void add_to_subdir(FILE *disk, fat12_t fat12, char *filename, int size,
                   int free_index, int index, char *dirpath) {
  char *next_dir = strtok(NULL, "/");
  if (next_dir == NULL) {
    NULL;
    // go through this dirs sectors until space
    // is found, then add the directory entry

    // if no space, find a free space in the
    // fat table and add a new sector.
  }
  // keep looking for the next directory
}
void copy_to_subdir(FILE *disk, FILE *source, fat12_t fat12, char *filename,
                    int size, char *dirpath) {
  // check if the filename already exists
  for (int i = 0; i < DIRS_IN_ROOT; i++) {
    if (strcmp(filename_ext(fat12.root.dirs[i]), filename) == 0) {
      printf("Error: file already exists.\n");
      exit(1);
    }
  }
  // can just keep this part the same. Only annoying part is figuring out where
  // to put the directory entry.
  ushort free_index = next_free_index(fat12.fat, 2);
  printf("Writing to Disk\n");
  write_file(source, disk, fat12, free_index, size);
  char *first = strtok(dirpath, "/");

  for (int i = 0; i < fat12.root.size; i++) {
    directory_t dir = fat12.root.dirs[i];
    switch (should_skip_dir(dir)) {
    case 1 ... 2:
      continue;
    case 3:
      printf("%s not found in root directory.\n", first);
      exit(1);
    default:
      NULL;
      char *filename = filename_ext(dir);
      if (strcmp(filename, first) == 0) {
        printf("Found %s\n", filename);
        ushort index = bytes_to_ushort(dir.first_cluster);
        add_to_subdir(disk, fat12, filename, size, free_index, index, dirpath);
      }
    }
  }
}

int file_size(FILE *file) {
  fseek(file, 0L, SEEK_END);
  int size = ftell(file);
  fseek(file, 0L, SEEK_SET);
  return size;
}

int main(int argc, char *argv[]) {
  FILE *disk = fopen(argv[1], "rb+");
  char *filename = (argc == 3) ? argv[2] : argv[3];
  char *dir = (argc == 3) ? 0x00 : argv[2];

  if (disk == NULL) {
    printf("Error opening disk image.\n");
  }
  FILE *source = fopen(filename, "rb");
  // make the filename uppercase
  for (int i = 0; filename[i]; i++) {
    filename[i] = toupper(filename[i]);
  }

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
  if (dir == 0x00) {
    copy_to_root(disk, source, fat12, filename, size);
  } else {
    printf("Copying to subdirectory\n");
    printf("Not implemented yet\n");
  }
  free_fat12(fat12);
  return 0;
}
