#ifndef HEADER
#define HEADER
#include <sys/stat.h>

typedef struct __attribute__((__packed__)) {
  unsigned char name[100];
  unsigned char mode[8];
  unsigned char uid[8];
  unsigned char gid[8];
  unsigned char size[12];
  unsigned char mtime[12];
  unsigned char chksum[8];
  unsigned char typeflag;
  unsigned char linkname[100];
  unsigned char magic[6];
  unsigned char version[2];
  unsigned char uname[32];
  unsigned char gname[32];
  unsigned char devmajor[8];
  unsigned char devminor[8];
  unsigned char prefix[155];
  unsigned char unused[12];
} TarHeader;

TarHeader *init_header();

char *extract_name(TarHeader *header, char *full_name);
void populate_header_from_file(const char *path, TarHeader *header);
void populate_name(const char *path, struct stat *path_stat, TarHeader *header);
void populate_mode(struct stat *path_stat, TarHeader *header);
void print_tar_header(const TarHeader *header);
void permissions_to_string(char *octal_str, char *str, TarHeader *header);

#endif
