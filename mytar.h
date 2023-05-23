#ifndef MYTAR
#define MYTAR
#include <stdbool.h>

#define RW_ALL 0666
#define RWX_ALL 0777
#define OCTAL_SIZE 8

typedef struct {
  bool create;
  bool list;
  bool extract;
  bool verbose;
  bool strict;
  char *tarfile;
  char **paths;
  int n_paths;
} Flags;

#endif
