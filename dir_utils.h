#include <stdint.h>
#include <stdio.h>

typedef struct dir_buf_t {
  char *buf;
  int size;
} dir_buf_t;
// FAT12 directory entry.
typedef struct directory_t {
  uint8_t filename[8];
  uint8_t extension[3];
  uint8_t attribute;
  uint8_t reserved[2];
  uint8_t creation_time[2];
  uint8_t creation_date[2];
  uint8_t last_access_date[2];
  uint8_t ignore[2];
  uint8_t last_modified_time[2];
  uint8_t last_modified_date[2];
  uint8_t first_cluster[2];
  uint8_t file_size[4];
} directory_t;

char *read_bytes_at(FILE *disk, int n, int offset);
directory_t *read_dir_at(char *buf, int root, int index);
directory_t *get_dir_list(char *buf, int limit);
int uint8s_to_int(uint8_t *ls, int size);
uint16_t fat_entry(char *fat_table, int n);

dir_buf_t *dir_buf_from_fat(FILE *disk, char *fat_table, int index);

int is_parent_or_cur(directory_t);

int count_files(FILE *disk, char *fat_table, dir_buf_t *dir_buf);
