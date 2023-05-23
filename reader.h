#include "header.h"
#include "writer.h"
#include <stdbool.h>

#ifndef READER
#define READER

typedef struct {
  TarHeader *header;
  unsigned char file_buf[USTAR_BLOCK];
} Entry;

typedef struct {

  int src_fd;
  int dst_fd;
  bool is_strict;
  Entry *current_entry;

} Reader;

void reader_init(Reader *reader, bool strict);
void reader_translate_to_file(Reader *reader);
bool is_end_of_archive(TarHeader *header);
int reader_cycle_entry(Reader *reader);
void reader_skip_file_contents(Reader *reader);

#endif
