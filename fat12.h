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

typedef struct dir_list_t {
  directory_t *dirs;
  int size;
} dir_list_t;

uint16_t fat_entry(byte *fat_table, int n);

// fucntions for various filesystem actions.
void copy_file(FILE *src_disk, FILE *out, byte *fat_table, int index, int size);
int count_files(FILE *disk, byte *fat_table, dir_list_t dirs);
dir_list_t dir_from_fat(FILE *disk, byte *fat_table, int index);

// functions for reading directories out of the filesystem
directory_t *sector_dirs(FILE *disk, int sector);
directory_t *root_dirs(FILE *disk);

// helper functions for getting boot sector and
// fat table buffers.
byte *boot_sector_buf(FILE *disk);
byte *fat_table_buf(FILE *disk);

// combines the filename and extension sections
// of a directory entry into a single string.
char *filename_ext(directory_t dir);
