/* writer.c
 * This file is in charge of handling write operations when creating tarfiles.
 * It abstracts this process through the use of the writer struct defined in
 * writer.h
 */
#include "writer.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

Writer *writer_init(Writer *writer) {

  writer->header = NULL;

  writer->src_fd = 0;

  writer->dst_fd = 1;

  writer->buffer_offset = 0;

  return writer;
}

/* Returns the true index in the buffer as the buffer is handeled in blocks of
 * 512 */
int get_buffer_index(Writer *writer) {
  return writer->buffer_offset * USTAR_BLOCK;
}

/* Flushes any content in the buffer to the file */
void writer_flush(Writer *writer) {

  if (write(writer->dst_fd, writer->buf,
            (writer->buffer_offset * USTAR_BLOCK)) == -1) {
    perror("Failed to flush buffer");
    exit(EXIT_FAILURE);
  }

  writer->buffer_offset = 0;
}

/* Adds padding to the buffer in order to adhere to USTAR spec.*/
void writer_pad(Writer *writer) {
  /* If not enough space in buffer for padding */

  if ((writer->buffer_offset + 2) > (NUM_HUNKS - 1)) {
    writer_flush(writer);
  }

  /* write two 512 byte blocks to buffer */
  memset(writer->buf + get_buffer_index(writer), 0, USTAR_BLOCK * 2);

  writer->buffer_offset += 2;
}

/* Writes file contents to intermediary buffer, only calls flush if buffer
 * full.*/
void writer_write_file(Writer *writer) {
  int bytes_read;
  int difference = 0;
  int current_index;

  if (lseek(writer->src_fd, 0, SEEK_SET) == -1) {
    perror("Failed to seek file. Exiting...\n");
    exit(EXIT_FAILURE);
  };

  /* Fill the buffer with file content */
  while ((bytes_read =
              read(writer->src_fd, writer->buf + get_buffer_index(writer),
                   (NUM_HUNKS - writer->buffer_offset) * USTAR_BLOCK)) > 0) {

    /* if we need to add padding to the buffer block */
    if (bytes_read % USTAR_BLOCK != 0) {
      difference = USTAR_BLOCK - (bytes_read % USTAR_BLOCK);
      current_index = get_buffer_index(writer) + bytes_read;

      memset(writer->buf + current_index, 0, difference);
    }

    writer->buffer_offset += (difference + bytes_read) / USTAR_BLOCK;
    if (writer->buffer_offset == NUM_HUNKS) {
      writer_flush(writer);
    }
  }

  if (bytes_read == -1) {
    perror("Failed to read src file.");
    exit(EXIT_FAILURE);
  }
}

void writer_write_header(Writer *writer) {

  if (write(writer->dst_fd, writer->header, sizeof(*writer->header)) == -1) {
    perror("Failed to write header to destination file");
    exit(EXIT_FAILURE);
  }
}
