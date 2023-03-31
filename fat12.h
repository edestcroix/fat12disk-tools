#include "byte.h"

#define ROOT 19
#define ROOT_DIR_SIZE (14 * 512)
#define DIRS_IN_ROOT 224

#define SECTOR_SIZE 512
#define SECTOR_OFFSET 31

#define DIR_SIZE 32
#define DIRS_PER_SECTOR 16

#define LAST_SECTOR 0xFF8

#define DIR_MASK 0x10
#define LABEL_MASK 0x08
#define LONG_NAME 0x0F

#define DOT 0x2E

#define FILE_FREE 0xE5

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

/* wrapper for a list of directories, to keep track of size.
 * (Decided to do it this way instead of a linked list.)
 * dirs is a pointer to the first directory in the list,
 * which is just a sequential block of directory structs in memory.*/
typedef struct dir_list_t {
  directory_t *dirs;
  int size;
} dir_list_t;

typedef struct fat_table_t {
  byte *table;
  int start;
  int size;
  int valid_sectors;
} fat_table_t;

typedef struct fat12_t {
  byte *boot_sector;
  fat_table_t fat;
  dir_list_t root;
  uint num_sectors;
  uint sector_size;
  uint num_files;
  uint free_space;
} fat12_t;

ushort fat_entry(byte *fat_table, int n);

// functions for various filesystem actions.
void copy_file(FILE *src_disk, FILE *out, byte *fat_table, int index, int size);
int count_files(FILE *disk, byte *fat_table, dir_list_t dirs);
dir_list_t dir_from_fat(FILE *disk, byte *fat_table, int index);

int should_skip_dir(directory_t dir);

// functions for reading directories out of the filesystem
directory_t *sector_dirs(FILE *disk, int sector);
directory_t *root_dirs(FILE *disk);

// helper functions for getting boot sector and fat table buffers.
byte *boot_sector_buf(FILE *disk);
byte *fat_table_buf(FILE *disk);

// combines the filename and extension sections
// of a directory entry into a single string.
char *filename_ext(directory_t dir);

int free_space(byte *fat_table, int num_sectors);

fat12_t fat12_from_file(FILE *disk);
void free_fat12(fat12_t fat12);
