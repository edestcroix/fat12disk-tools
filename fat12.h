#include "byte.h"
#include <stdio.h>

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

typedef struct buf_t {
  byte *buf;
  int size;
} buf_t;

byte *boot_sector_buf(FILE *disk);
byte *fat_table_buf(FILE *disk);
byte *root_dir_buf(FILE *disk);
uint16_t fat_entry(byte *fat_table, int n);
char *filename_ext(directory_t dir);
buf_t cluster_buf(FILE *disk, byte *fat_table, int index, int steps);
void copy_file(FILE *src_disk, FILE *out, byte *fat_table, int index, int size);
