#ifndef WRITER
#define WRITER

#include "header.h"
#include <stdio.h>

#define USTAR_BLOCK 512
#define NUM_HUNKS 8
#define BUFFER_SIZE NUM_HUNKS *USTAR_BLOCK

typedef unsigned char buffer[BUFFER_SIZE];

typedef struct {

  TarHeader *header;
  int src_fd;
  int dst_fd;
  buffer buf;
  int buffer_offset;

} Writer;

Writer *writer_init(Writer *writer);
int get_buffer_index(Writer *writer);
void writer_flush(Writer *writer);
void writer_pad(Writer *writer);
void writer_write_file(Writer *writer);
void writer_write_header(Writer *writer);

#endif
