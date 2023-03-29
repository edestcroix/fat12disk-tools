#include "byte.h"
#include <stdint.h>
#include <stdio.h>

typedef uint16_t fat_entry_t;

typedef struct dir_buf_t {
  byte *buf;
  int size;
} dir_buf_t;

// FAT12 directory entry.
typedef struct directory_t {
  byte filename[8];
  byte extension[3];
  byte attribute;
  byte reserved[2];
  byte creation_time[2];
  byte creation_date[2];
  byte last_access_date[2];
  byte ignore[2];
  byte last_modified_time[2];
  byte last_modified_date[2];
  byte first_cluster[2];
  byte file_size[4];
} directory_t;

directory_t *read_dir_at(byte *buf, int index);

directory_t *get_dir_list(byte *buf, int limit);

fat_entry_t fat_entry(byte *fat_table, int n);

dir_buf_t *dir_buf_from_fat(FILE *disk, byte *fat_table, int index);

int is_parent_or_cur(directory_t);

int count_files(FILE *disk, byte *fat_table, dir_buf_t *dir_buf);
