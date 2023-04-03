#include "fat12.h"
#include <assert.h>
#include <ctype.h>

// TODO:- Find places where pointers aren't being freed (If any).
//      - Add some parsing to the input arguments to make sure
//        they are converted to the format the code expects.
typedef struct dir_info {
  char filename[50];
  int size;
  ushort first_cluster;
} dir_info_t;

void write_to_disk(FILE *disk, void *buffer, int address, int block_size,
                   int write_size) {

  fseek(disk, address, SEEK_SET);
  if (fwrite(buffer, block_size, write_size, disk) < write_size) {
    printf("Error: failed to write to disk.\n");
    exit(1);
  }
}

// starting at index+1, search the fat table for the next free sector,
// use index i + 1 because the current index might not be updated yet.
ushort next_free_index(fat_table_t fat, int index) {
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
  if (size < SECTOR_SIZE) {
    byte buf[size];
    fread(buf, 1, size, src_file);
    write_to_disk(dest_disk, buf, sector, 1, size);
    update_fat_table(fat12.fat.table, 0xFFF, index);
  } else {
    byte buf[SECTOR_SIZE];
    fread(buf, 1, SECTOR_SIZE, src_file);
    write_to_disk(dest_disk, buf, sector, 1, SECTOR_SIZE);
    ushort next_index = next_free_index(fat12.fat, index);
    write_file(src_file, dest_disk, fat12, next_index, size - SECTOR_SIZE);
    update_fat_table(fat12.fat.table, next_index, index);
  }
}

directory_t create_dir(dir_info_t dir_info) {
  char *filename = dir_info.filename;
  ushort start_index = dir_info.first_cluster;
  int size = dir_info.size;
  directory_t dir;
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

  ushort time_stamp =
      (time.tm_hour << 11) | (time.tm_min << 5) | (time.tm_sec / 2);
  memcpy(&dir.creation_time, &time_stamp, 2);
  ushort date_stamp =
      ((time.tm_year - 80) << 9) | (time.tm_mon << 5) | time.tm_mday;
  // TODO: Last modified times should be set to the times
  // of the file being copied, not the current time.
  // (Have to add a new parameter to dir_info_t, and
  // read the file's times when opening it for reading.)
  memcpy(&dir.creation_date, &date_stamp, 2);
  memcpy(&dir.last_access_date, &date_stamp, 2);
  memcpy(&dir.last_modified_time, &time_stamp, 2);
  memcpy(&dir.last_modified_date, &date_stamp, 2);
  return dir;
}

void add_dir_to_sector(byte *sector, dir_info_t dir_info) {
  // sector will be a pointer to the position in the sector to add the directory
  directory_t dir = create_dir(dir_info);
  memcpy(sector, &dir, sizeof(directory_t));
}

// Extracts the top-most directory name out of dirpath, and also removes it from
// dirpath. Returns pointer to the extracted directory name.
char *get_target(char *dirpath) {
  char *target = malloc(13);
  // check if there are any '\' left in dirpath
  for (int i = 0; i < 12; i++) {
    if (dirpath[i] == '/' || dirpath[i] == '\0') {
      target[i] = '\0';
      // move the rest of dirpath forward
      for (int j = i + 1; j < strlen(dirpath); j++) {
        dirpath[j - i - 1] = dirpath[j];
      }
      dirpath[strlen(dirpath) - i - 1] = '\0';
      break;
    }
    target[i] = dirpath[i];
    // also clear from dirpath
    if (i + 1 < 12) {
      dirpath[i] = dirpath[i + 1];
    }
  }
  return target;
}

void add_to_sector(FILE *disk, fat12_t fat12, int index, dir_info_t dir_info,
                   char *dirpath);

void find_next_dir(FILE *disk, fat12_t fat12, int index, dir_info_t dir_info,
                   char *dirpath, char *target) {

  int sector_num = index + SECTOR_OFFSET;
  byte *sector = read_sector(disk, sector_num);
  for (int i = 0; i < SECTOR_SIZE; i += sizeof(directory_t)) {
    directory_t *dir = (directory_t *)(sector + i);
    switch (should_skip_dir(*dir)) {
    case 3:
      return;
    case 1 ... 2:
      continue;
    default:
      if (dir->filename[0] == DOT) {
        continue;
      }
      // check if one of the dirs is the target on the dirpath.
      char *dirname = filename_ext(*dir);
      if (dir->attribute & DIR_MASK) {
        // if the dir is a directory, check if it is the target
        // and if it is, recurse into that directory.
        if (strncmp(target, dirname, 12) == 0) {
          ushort next_index = bytes_to_ushort(dir->first_cluster);
          printf("Found directory %s\n", target);
          add_to_sector(disk, fat12, next_index, dir_info, dirpath);
          free(sector);
          return;
        }
      }
    }
  }
}

void add_to_sector(FILE *disk, fat12_t fat12, int index, dir_info_t dir_info,
                   char *dirpath) {

  char *target = get_target(dirpath);
  int sector_num = index + SECTOR_OFFSET;
  byte *sector = read_sector(disk, sector_num);

  if (strncmp(target, dir_info.filename, 12) == 0) {
    for (int i = 0; i < SECTOR_SIZE; i += 32) {
      directory_t *dir = (directory_t *)(sector + i);
      switch (should_skip_dir(*dir)) {
      case 2 ... 3:
        add_dir_to_sector(sector, dir_info);
        write_to_disk(disk, sector, sector_num, SECTOR_SIZE, 1);
        free(sector);
        return;
      default:
        NULL;
        char *dirname = filename_ext(*dir);
        if (strncmp(dir_info.filename, dirname, 8) == 0) {
          printf("Error: File already exists\n");
          free(sector);
          free(target);
          exit(1);
        }
      }
    }
  } else {
    find_next_dir(disk, fat12, index, dir_info, dirpath, target);
  }

  // if we get here, the sector is full, check the next one.
  if (index >= LAST_SECTOR) {
    // if the next sector is the last sector, we need to add a new sector.
    // Create a new sector and add the directory to it, then write this sector
    // into the disk at the first available index.
    ushort free_index = next_free_index(fat12.fat, 1);
    byte new_sector[SECTOR_SIZE];
    memset(new_sector, 0, SECTOR_SIZE);
    add_dir_to_sector(new_sector, dir_info);
    write_to_disk(disk, new_sector, sector_num * SECTOR_SIZE, SECTOR_SIZE, 1);
    update_fat_table(fat12.fat.table, free_index, index);
  } else if (index != 0) {
    // move to the next sector of this directory.
    int next_index = fat_entry(fat12.fat.table, index);
    // NOTE: Potential for recursion problems here, but it should terminate
    // once a LAST_SECTOR is reached in the fat table.
    add_to_sector(disk, fat12, next_index, dir_info, dirpath);
  }
}

void add_to_tree(FILE *disk, FILE *source, fat12_t fat12, dir_info_t dir_info,
                 char *dirpath) {
  char *filename = dir_info.filename;
  int size = dir_info.size;
  ushort file_index = dir_info.first_cluster;

  // check if the filename already exists
  for (int i = 0; i < DIRS_IN_ROOT; i++) {
  }

  // read out the directory path until the first / character
  char *target = get_target(dirpath);

  for (int i = 0; i < fat12.root.size; i++) {
    directory_t dir = fat12.root.dirs[i];
    switch (should_skip_dir(dir)) {
    case 1:
      continue;
    case 2 ... 3:
      if (strncmp(target, filename, 8) == 0) {
        directory_t new_dir = create_dir(dir_info);
        // write the directory to the root directory
        memcpy(fat12.root.dirs + i, &new_dir, sizeof(directory_t));
        write_to_disk(disk, fat12.root.dirs, 19 * 512, sizeof(directory_t),
                      DIRS_IN_ROOT);
        printf("Added %s to root directory.\n", filename);
        free(target);
        return;
      } else {
        printf("%s not found in root directory.\n", target);
        free(target);
        exit(1);
        return;
      }
    default:
      if (dir.filename[0] == DOT) {
        continue;
      }
      NULL;
      char *dirname = filename_ext(dir);
      if (strcmp(filename, dirname) == 0) {
        printf("Error: file already exists.\n");
        exit(1);
      }
      if (strcmp(dirname, target) == 0) {
        printf("Found %s\n", dirname);
        ushort index = bytes_to_ushort(dir.first_cluster);
        add_to_sector(disk, fat12, index, dir_info, dirpath);
        return;
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

  ushort free_index = next_free_index(fat12.fat, 2);
  printf("Writing to Disk\n");
  // write here, but the FAT table isn't written back to the disk until the
  // end of the program, so if something goes wrong, the disk will be left in
  // a consistent state, and the copied file can just be overwritten.
  write_file(source, disk, fat12, free_index, size);

  dir_info_t dir_info = {.size = size, .first_cluster = free_index};
  strncpy(dir_info.filename, filename, 13);

  if (dir == 0x00) {
    char dirpath[200];
    strcpy(dirpath, filename);
    add_to_tree(disk, source, fat12, dir_info, dirpath);
  } else {
    printf("Copying %s to subdirectory: %s\n", filename, dir);
    char dirpath[200];
    strcpy(dirpath, dir);
    strcat(dirpath, "/");
    strcat(dirpath, filename);
    add_to_tree(disk, source, fat12, dir_info, dirpath);
  }

  printf("Write Complete\nUpdating FAT Table\n");
  write_to_disk(disk, fat12.fat.table, 512 * 1, fat12.fat.size, 1);
  free_fat12(fat12);
  return 0;
}
