/* File containing utilites for
 * interacting with fat12 disk images.
 */
#include "fat12.h"
#include "byte.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// wrapper around common code.
byte *read_sector(FILE *disk, int sector_num) {
  return read_bytes(disk, 512, sector_num * 512);
}

/* Retrieves the 12-bit value stored
 * in the fat table at index n. */
uint16_t fat_entry(byte *fat_table, int n) {
  // first, read the 2 bytes starting at
  // (3*n/2)th byte of the table
  unsigned char entry[2];
  memcpy(entry, fat_table + ((3 * n) / 2), 2);
  uint16_t b1 = entry[0];
  uint16_t b2 = entry[1];
  return (n % 2 == 0) ? ((0x00f & b2) << 8) | b1 : b2 << 4 | ((0xf0 & b1) >> 4);
}

/* Copies the file starting at the sector in the FAT-12
 * filesystem in src_disk corresponding to index into the out file
 * on the host filesystem. */
void copy_file(FILE *src_disk, FILE *out, byte *fat_table, int index,
               int size) {
  uint16_t next_index = fat_entry(fat_table, index);
  int sector_num = index + SECTOR_OFFSET;

  if (next_index >= LAST_SECTOR || next_index == 0x00) {
    // this is the end of the chain,
    // read the sector and return it
    byte *sector = read_sector(src_disk, sector_num);
    fwrite(sector, size, 1, out);
    free(sector);
    return;
  }
  byte *sector = read_sector(src_disk, sector_num);
  fwrite(sector, SECTOR_SIZE, 1, out);
  copy_file(src_disk, out, fat_table, next_index, size - SECTOR_SIZE);
  free(sector);
}

directory_t read_dir(FILE *disk, int address) {
  directory_t dir;
  fseek(disk, address, SEEK_SET);
  fread(&dir, sizeof(directory_t), 1, disk);
  return dir;
}

// allows reading limit amount of directory entries from
// the sector specified. Allows reading arbitrary number of
// directory entriess, so it is not exposed outside this
// file, only through wrapper functions that impose
// limits on the number of directory entries that can be read.
directory_t *read_dirs(FILE *disk, int sector, int limit) {
  directory_t *dir_list = malloc(limit * sizeof(directory_t));
  int add_at = 0;
  int last_dir_lf = 0;
  for (int i = 0; i < limit; i++) {
    // check if the first byte of filename
    // is 0x00, in which case the rest of the
    // directory entries are empty
    directory_t dir = read_dir(disk, sector * SECTOR_SIZE + i * DIR_SIZE);
    if (dir.filename[0] == FILE_FREE) {
      continue;
    } else if (dir.filename[0] == 0x00) {
      // set the rest of the list to
      // only 0x00 bytes, in case this bit
      // of memory is being reused. Otherwise,
      // the rest of the list might have old
      // data in it.
      memset(dir_list + add_at, 0x00, (limit - i) * sizeof(directory_t));
      break;
    } else {
      if (bytes_to_int(dir.first_cluster, 2) <= 1) {
        continue;
      }
      // if the attribute is 0x0F, it's a long filename,
      // so skip it, and set the last_dir_lf flag
      if (dir.attribute == LONG_NAME) {
        last_dir_lf = 1;
        continue;
      }
      if (dir.attribute & LABEL_MASK) {
        continue;
      }
      if (dir.attribute & DIR_MASK) {
        // if the attribute is 0x10, it's a directory,
        // and if the last_dir_lf flag is set,
        // then it's part of a long filename, so
        // just skip it.
        // NOTE: Unclear if only the long
        // filename directory entries should be
        // skipped, or if the entire directory should
        // be. Right now, I'm doing the latter.
        if (last_dir_lf) {
          last_dir_lf = 0;
          continue;
        }
      }
      dir_list[add_at++] = dir;
    }
  }
  return dir_list;
}

// reads the 16 directory entries in the sector specified,
directory_t *sector_dirs(FILE *disk, int sector) {
  return read_dirs(disk, sector, DIRS_PER_SECTOR);
}
// reads the 224 directory entries in the root directory
directory_t *root_dirs(FILE *disk) {
  return read_dirs(disk, ROOT, DIRS_IN_ROOT);
}

/* performs a complete filesystem traversal,
 * counting every file encountered. */
int count_files(FILE *disk, byte *fat_table, dir_list_t dirs) {
  int num = 0, num_dirs = dirs.size;
  directory_t *dir_list = dirs.dirs;
  for (int i = 0; i < num_dirs; i++) {
    directory_t dir = dir_list[i];
    if (dir.filename[0] == 0x00) {
      break;
    } else if (dir.filename[0] == FILE_FREE) {
      continue;
    } else if (dir.filename[0] == DOT) {
      continue;
    } else if (dir.attribute & DIR_MASK) {
      uint16_t index = bytes_to_int(dir.first_cluster, 2);
      if (index > 1) {
        dir_list_t next_dirs = dir_from_fat(disk, fat_table, index);
        num += count_files(disk, fat_table, next_dirs);
      }
    } else if (!(dir.attribute & DIR_MASK)) {
      num++;
      continue;
    }
  }
  return num;
}

/* Creates a list of directory_t structs contained in
 * the directory starting at index in the FAT Table
 * Reads all the directories from the sector into
 * the list, then recurses into the next sector. */
dir_list_t dir_from_fat(FILE *disk, byte *fat_table, int index) {
  uint16_t next_index = fat_entry(fat_table, index);
  int sector_num = 33 + index - 2;

  if (next_index >= LAST_SECTOR) {
    directory_t *dirs = sector_dirs(disk, sector_num);
    dir_list_t dir_list;
    dir_list.dirs = dirs;
    dir_list.size = 16;
    return dir_list;
  }
  directory_t *dirs = sector_dirs(disk, sector_num);
  dir_list_t dir_list;
  dir_list_t next_dir_list = dir_from_fat(disk, fat_table, next_index);
  dir_list.dirs = malloc((16 + next_dir_list.size) * sizeof(directory_t));
  dir_list.size = 16 + next_dir_list.size;
  memcpy(dir_list.dirs, dirs, 16 * sizeof(directory_t));
  memcpy(dir_list.dirs + 16, next_dir_list.dirs,
         next_dir_list.size * sizeof(directory_t));
  free(dirs);
  return dir_list;
}

byte *boot_sector_buf(FILE *disk) {
  fseek(disk, 0, SEEK_SET);
  byte *buf = (byte *)malloc(SECTOR_SIZE * sizeof(byte));
  fread(buf, SECTOR_SIZE, 1, disk);
  return buf;
}

byte *fat_table_buf(FILE *disk) {
  byte *boot_sector = boot_sector_buf(disk);
  int fat_size = boot_sector[22] + (boot_sector[23] << 8);
  int reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);
  int fat_start = reserved_sectors;
  int fat_size_bytes = fat_size * SECTOR_SIZE;
  byte *fat_table = (byte *)malloc(fat_size_bytes * sizeof(byte));
  fseek(disk, SECTOR_SIZE * fat_start, SEEK_SET);
  fread(fat_table, fat_size_bytes, 1, disk);
  return fat_table;
}

char *filename_ext(directory_t dir) {
  char *filename = (char *)malloc(13 * sizeof(char));
  memset(filename, 0, 12);
  strncpy(filename, bytes_to_filename(dir.filename), 8);
  strncat(filename, ".", 2);
  strncat(filename, (char *)dir.extension, 3);
  return filename;
}
