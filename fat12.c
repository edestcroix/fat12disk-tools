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
  return read_bytes(disk, SECTOR_SIZE, sector_num * SECTOR_SIZE);
}

/* Retrieves the 12-bit value stored
 * in the fat table at index n. */
uint16_t fat_entry(byte *fat_table, int n) {
  // first, read the 2 bytes starting at
  // (3*n/2)th byte of the table
  unsigned char entry[2];
  memcpy(entry, fat_table + ((3 * n) / 2), 2);
  if (n % 2 == 0) {
    /* if n is even, the first byte of the entry is
     * in position 0, and we have to get the low 4
     * bytes of the second byte, shift it left
     * 8 bits, and OR it with the first byte. */
    uint16_t first_byte = entry[0];
    uint16_t second_byte = entry[1];
    return ((0x00f & second_byte) << 8) | first_byte;
  } else {
    /* if n is odd, the second byte of the entry is
     * in position 1, and we have to get the high 4
     * bytes of the first byte, shift it right 4,
     * and OR it with the second byte shifted left 4 */
    uint16_t first_byte = entry[0];
    uint16_t second_byte = entry[1];
    return second_byte << 4 | ((0xf0 & first_byte) >> 4);
  }
}

/* Copies the file starting at the sector in the FAT-12
 * filesystem in src_disk corresponding to index into the out file
 * on the host filesystem. */
void copy_file(FILE *src_disk, FILE *out, byte *fat_table, int index,
               int size) {
  uint16_t next_index = fat_entry(fat_table, index);
  int sector_num = index + SECTOR_OFFSET;
  if (next_index == 0x00) {
    printf("Error: next_index is 0x00 in copy_file\n");
    exit(1);
  }

  if (next_index >= LAST_SECTOR) {
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
/*TODO: write_file, which writes a file from the host
 * filesystem to the FAT-12 filesystem. Idea for this
 * is to find the first free sector in the FAT table,
 * then write the first 512 bytes to that sector,
 * and recurse on the rest of the file and the next
 * free sector. The recursive call returns the value of the next free
 * sector, which is then written to the FAT table. */
directory_t *read_dir(FILE *disk, int address) {
  directory_t *dir = malloc(sizeof(directory_t));
  fseek(disk, address, SEEK_SET);
  fread(dir, sizeof(directory_t), 1, disk);
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
    directory_t dir = *read_dir(disk, sector * SECTOR_SIZE + i * 32);
    if (dir.filename[0] == FILE_FREE) {
      continue;
    } else if (dir.filename[0] == 0x00) {
      break;
    } else {
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
    } else if (!(dir.attribute & DIR_MASK)) {
      num++;
      continue;
      // NOTE: I think the correct way to check
      //  for the root or cur dir
      //  is to check if the first_cluster is 1 or 2,
      //  but not sure.
    } else if (dir.filename[0] == DOT || dir.filename[0] == FILE_FREE) {
      continue;
    } else {
      uint16_t index = bytes_to_int(dir.first_cluster, 2);
      if (index > 1) {
        char dirname[9];
        strncpy(dirname, bytes_to_filename(dir.filename), 8);
        dir_list_t next_dirs = dir_from_fat(disk, fat_table, index);
        num += count_files(disk, fat_table, next_dirs);
      }
    }
  }
  free(dir_list);
  return num;
}

/* Creates a list of directory_t structs contained in
 * the directory starting at index in the FAT Table
 * Reads all the directories from the sector into
 * the list, then recurses into the next sector. */
dir_list_t dir_from_fat(FILE *disk, byte *fat_table, int index) {
  uint16_t next_index = fat_entry(fat_table, index);
  int sector_num = index + SECTOR_OFFSET;

  if (next_index >= LAST_SECTOR) {
    directory_t *dirs = sector_dirs(disk, sector_num);
    dir_list_t dir_list;
    dir_list.dirs = dirs;
    dir_list.size = DIRS_PER_SECTOR;
    return dir_list;
  }
  directory_t *dirs = sector_dirs(disk, sector_num);
  dir_list_t next_dir_list = dir_from_fat(disk, fat_table, next_index);
  int new_size = DIRS_PER_SECTOR + next_dir_list.size;
  dir_list_t dir_list = (dir_list_t){
      .dirs = malloc((new_size) * sizeof(directory_t)), .size = new_size};

  memcpy(dir_list.dirs, dirs, DIRS_PER_SECTOR * sizeof(directory_t));
  memcpy(dir_list.dirs + DIRS_PER_SECTOR, next_dir_list.dirs,
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
