/* File containing utilites for interacting with fat12 disk images. */
#include "fat12.h"

FILE *open_disk(char *filename, char *attr) {
  FILE *disk = fopen(filename, attr);
  if (disk == NULL) {
    printf("ERROR: Disk image %s does not exist\n", filename);
    exit(1);
  }
  return disk;
}

void read_from_disk(FILE *disk, void *buf, int address, int block_size,
                    int read_amt) {
  fseek(disk, address, SEEK_SET);
  if (fread(buf, block_size, read_amt, disk) < read_amt) {
    printf("Error reading from disk.\n");
    exit(1);
  }
}

// reads exactly SECTOR_SIZE bytes from the disk, at the
// sector_num * SECTOR_SIZE offset from the start of the disk.
byte *read_sector(FILE *disk, int sector_num) {
  return read_bytes(disk, SECTOR_SIZE, sector_num * SECTOR_SIZE);
}

/* checks if the index from the FAT table is the value(s) that indicate the end
 * of a file. Additionally, throws error if index is <= 1, as this should never
 * be encountered normally. */
int last_sector(int index, char *exit_msg) {
  if (index <= 1) {
    printf("Error at %s: reserved or invalid FAT entry.\n", exit_msg);
    exit(1);
  }
  return index >= LAST_SECTOR;
}

/* Retrieves the 12-bit value stored in the fat table at index n. If n is even,
 * the lower byte of the index is b1, and the remaining 4 bits are the upper 4
 * bits of b2. (the bits from b2 are shifted right 8 bits to be added to b1
 * using logical OR.) If n is odd, the upper 4 bits of b1 are the lower 4 bits
 * of the index, and b2 is the upper byte of the index. (b2 gets shifted left 4
 * bits to make room for the bits from b1) */
ushort fat_entry(byte *fat_table, int n) {
  ushort b1 = fat_table[3 * n / 2], b2 = fat_table[3 * n / 2 + 1];
  return (n % 2 == 0) ? ((0x00f & b2) << 8) | b1 : b2 << 4 | ((0xf0 & b1) >> 4);
}

int is_cur_or_parent(directory_t dir) {
  // check for either . or .. filenames
  int dots = (dir.filename[0] == DOT) + (dir.filename[1] == DOT);
  // check if there is a space after the dots.
  return dots <= 2 && dir.filename[dots] == 0x20;
}

/* Returns status code to determine whether a directory entry should be skipped.
 * 0: do not skip
 * 1: volume_label (sometimes this shouldn't be skipped)
 * 2: free entry, or current dir/parent dir, skip
 * 3: free entry, skip and end search. */
int should_skip_dir(directory_t dir) {
  if (dir.filename[0] == 0x00) {
    return 3;
  } else if (dir.filename[0] == FILE_FREE) {
    return 2;
  } else if (dir.attribute == LABEL_MASK) {
    return 1;
  } else if (bytes_to_ushort(dir.first_cluster) <= 1) {
    return 2;
  } else if (is_cur_or_parent(dir)) {
    return 2;
  } else {
    return 0;
  }
}

directory_t read_dir(FILE *disk, int address) {
  directory_t dir;
  read_from_disk(disk, &dir, address, sizeof(directory_t), 1);
  return dir;
}

/* Allows reading "limit" amount of directory entries from the sector specified.
 * It allows reading an arbitrary number of directory entriess, so it is not
 * exposed outside this file, only through wrapper functions that impose limits
 * on the number of directory entries that can be read. */
directory_t *read_dirs(FILE *disk, int sector, int limit) {
  directory_t *dir_list = malloc(limit * sizeof(directory_t));
  int add_at = 0;
  for (int i = 0; i < limit; i++) {
    directory_t dir = read_dir(disk, sector * SECTOR_SIZE + i * DIR_SIZE);
    switch (should_skip_dir(dir)) {
    // only case 2 and 3 should be skipped. Don't skip volume lables,
    // because sometimes other functions need them.
    case 2:
      continue;
    case 3:
      // zero out the rest of the array before exit, to make
      // sure there are no garbage values leftover.
      memset(dir_list + add_at, 0x00, (limit - i) * sizeof(directory_t));
      return dir_list;
    default:
      dir_list[add_at++] = dir;
    }
  }
  return dir_list;
}

// reads the 16 directory entries in the sector specified. (sector value
// should be the number of the sector, not it's address in the disk.)
directory_t *sector_dirs(FILE *disk, int sector) {
  return read_dirs(disk, sector, DIRS_PER_SECTOR);
}
// reads the 224 directory entries in the root directory
directory_t *root_dirs(FILE *disk) {
  return read_dirs(disk, ROOT, DIRS_IN_ROOT);
}

/* performs a complete filesystem traversal, counting every file encountered. */
int count_files(FILE *disk, byte *fat_table, dir_list_t dirs) {
  int num = 0, num_dirs = dirs.size;
  directory_t *dir_list = dirs.dirs;
  for (int i = 0; i < num_dirs; i++) {
    directory_t dir = dir_list[i];
    switch (should_skip_dir(dir)) {
    case 1 ... 2:
      continue;
    case 3:
      free(dir_list);
      return num;
    default:
      if (dir.attribute & DIR_MASK) {
        ushort index = bytes_to_ushort(dir.first_cluster);
        if (index > 1) {
          dir_list_t next_dirs = dir_from_fat(disk, fat_table, index);
          num += count_files(disk, fat_table, next_dirs);
        }
      } else {
        num++;
      }
    }
  }
  free(dirs.dirs);
  return num;
}

/* Creates a list of directory_t structs contained in the directory starting at
 * index in the FAT Table Reads all the directories from the sector into the
 * list, then recurses into the next sector. */
dir_list_t dir_from_fat(FILE *disk, byte *fat_table, int index) {
  ushort next_index = fat_entry(fat_table, index);
  int sector_num = index + SECTOR_OFFSET;

  directory_t *dirs = sector_dirs(disk, sector_num);
  if (last_sector(next_index, "dir_from_fat")) {
    // a directory can hold 16 subdirectories,
    // but 2 will always be . and .., so only
    // 14 are available.
    dir_list_t dir_list = {.dirs = dirs, .size = 14};
    return dir_list;
  }

  dir_list_t next_dir_list = dir_from_fat(disk, fat_table, next_index);

  // combine the two lists. Only 14 directries will be added
  // to the list, because the other two are . and ..
  dir_list_t dir_list = {.size = 14 + next_dir_list.size};

  dir_list.dirs = malloc(dir_list.size * sizeof(directory_t));

  memcpy(dir_list.dirs, dirs, 14 * sizeof(directory_t));
  memcpy(dir_list.dirs + 14, next_dir_list.dirs,
         next_dir_list.size * sizeof(directory_t));

  free(dirs);
  free(next_dir_list.dirs);
  return dir_list;
}

fat_table_t fat_table(FILE *disk, byte *boot_sector) {
  int fat_size = boot_sector[22] + (boot_sector[23] << 8);
  int reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);
  int num_sectors = boot_sector[19] + (boot_sector[20] << 8);
  int fat_start = reserved_sectors;
  int fat_size_bytes = fat_size * SECTOR_SIZE;
  byte *fat_table = malloc(fat_size_bytes * sizeof(byte));
  read_from_disk(disk, fat_table, fat_start * SECTOR_SIZE, fat_size_bytes, 1);
  fat_table_t table = {.table = fat_table,
                       .size = fat_size_bytes,
                       .start = fat_start,
                       .valid_sectors = num_sectors - 32};
  return table;
}

char *filename_ext(directory_t dir) {
  char *filename = malloc(13 * sizeof(char));
  memset(filename, 0, 12);
  strncpy(filename, bytes_to_filename(dir.filename), 8);
  if (dir.extension[0] == 0x20) {
    return filename;
  }
  strncat(filename, ".", 2);
  strncat(filename, (char *)dir.extension, 3);
  return filename;
}

int free_space(byte *fat_table, int num_sectors) {
  int free_sectors = 0;
  // the first 2 entries in the fat table are reserved,
  // and there are 32 sectors that are not available for
  // data storage which should be excluded.
  for (int i = 0; i < num_sectors; i++) {
    free_sectors += (fat_entry(fat_table, i) == 0x000);
  }
  return (free_sectors - 32) * SECTOR_SIZE;
}

fat12_t fat12_from_file(FILE *disk) {
  byte *boot_sector = malloc(SECTOR_SIZE * sizeof(byte));
  read_from_disk(disk, boot_sector, 0, SECTOR_SIZE, 1);

  directory_t *root = root_dirs(disk);
  fat_table_t fat = fat_table(disk, boot_sector);
  dir_list_t root_list = {.dirs = root, .size = 224};
  int num_sectors = boot_sector[19] + (boot_sector[20] << 8);
  ushort bytes_per_sector = bytes_to_ushort(boot_sector + 11);
  int free_space_bytes = free_space(fat.table, num_sectors);
  int fat_size = boot_sector[22] + (boot_sector[23] << 8);
  fat_size *= SECTOR_SIZE;
  fat12_t fat12 = {.boot_sector = boot_sector,
                   .fat = fat,
                   .root = root_list,
                   .num_sectors = num_sectors,
                   .free_space = free_space_bytes,
                   .total_size = num_sectors * bytes_per_sector};
  return fat12;
}

void free_fat12(fat12_t fat12) {
  free(fat12.boot_sector);
  free(fat12.fat.table);
  free(fat12.root.dirs);
}
