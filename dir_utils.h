#include "byte.h"
#include "fat12.h"
#include <stdint.h>
#include <stdio.h>

typedef uint16_t fat_entry_t;

typedef struct dir_buf_t {
  byte *buf;
  int size;
} dir_buf_t;

directory_t *read_dir_at(byte *buf, int index);

directory_t *get_dir_list(byte *buf, int limit);

dir_buf_t dir_buf_from_fat(FILE *disk, byte *fat_table, int index);

int is_parent_or_cur(directory_t);

int count_files(FILE *disk, byte *fat_table, dir_buf_t dir_buf);
