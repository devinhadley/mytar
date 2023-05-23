/* reader.c
 *
 * This file is in charge of reading contents from a USTAR compliant tar header.
 * It abstracts this process through the use of the reader struct defined in
 * reader.h
 *
 */

#include "reader.h"
#include "header.h"
#include "mytar.h"
#include "writer.h"
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The header will contain the current parsed files header, and file_buf will
 * contain any file contents. */

void reader_init(Reader *reader, bool strict) {

  reader->src_fd = 0;
  reader->dst_fd = 1;

  reader->current_entry = NULL;
  reader->is_strict = strict;
}

/* Given a valid tar file this will:
 * Read the header into header parameter, and read file contents into file buf.
 */
void reader_translate_to_file(Reader *reader) {

  char *endptr;
  long size =
      strtol((char *)reader->current_entry->header->size, &endptr, OCTAL_SIZE);
  long handeled = 0;
  int bytes_read;
  int delta = 0;
  int read_size;
  errno = 0;

  if (errno != 0) {
    perror("strtol");
    exit(EXIT_FAILURE);
  }

  if (endptr == (char *)reader->current_entry->header->size) {
    fprintf(stderr, "No digits were found\n");
    exit(EXIT_FAILURE);
  }

  if (size % USTAR_BLOCK != 0) {
    delta = USTAR_BLOCK - (size % USTAR_BLOCK);
    size += delta;
  }

  while (handeled != size) {

    /* if this is the last iteration, lets not read in any padding */
    read_size =
        handeled + USTAR_BLOCK == size ? USTAR_BLOCK - delta : USTAR_BLOCK;

    bytes_read =
        read(reader->src_fd, reader->current_entry->file_buf, read_size);

    if (bytes_read == -1) {
      perror("failed to read src fd in when tranlating file: ");
      handeled += USTAR_BLOCK;
      continue;
    }

    if (write(reader->dst_fd, reader->current_entry->file_buf, read_size) ==
        -1) {
      perror("failed to write to destination when extracting: ");
    }

    handeled += USTAR_BLOCK;
  }
  /* in case we dont read the entire block */
  if (lseek(reader->src_fd, delta, SEEK_CUR) == -1) {
    perror("Lseek failed when reading.");
    exit(EXIT_FAILURE);
  }
}

/* Returns true if the end of the archive is reached */
bool is_end_of_archive(TarHeader *header) {
  static bool is_zero_header_initialized = false;
  static TarHeader zero_header;

  if (!is_zero_header_initialized) {
    memset(&zero_header, 0, sizeof(TarHeader));
    is_zero_header_initialized = true;
  }

  return memcmp(header, &zero_header, sizeof(TarHeader)) == 0;
}

/* Skips the file contents that follows a header */
void reader_skip_file_contents(Reader *reader) {
  long size;
  int delta;
  int new_offset;

  size = strtol((char *)reader->current_entry->header->size, NULL, OCTAL_SIZE);
  delta = (size % USTAR_BLOCK == 0) ? 0 : USTAR_BLOCK - (size % USTAR_BLOCK);
  new_offset = lseek(reader->src_fd, size + delta, SEEK_CUR);

  if (new_offset == -1) {
    perror("Failed to skip file contents in reader_cycle_entry");
    exit(EXIT_FAILURE);
  }
}

/* Returns true if a header's checksum is valid, false if not */
bool is_valid_checksum(TarHeader *header) {

  TarHeader *copied_header = malloc(sizeof(TarHeader));
  unsigned char *header_loc = (unsigned char *)header;
  long expected_checksum = strtol((char *)header->chksum, NULL, OCTAL_SIZE);
  int i;
  long actual_checksum = 0;
  bool result;

  memcpy(copied_header, header, sizeof(TarHeader));
  memset(copied_header->chksum, ' ', sizeof(copied_header->chksum));

  memset(header->chksum, ' ', sizeof(header->chksum));

  for (i = 0; i < sizeof(TarHeader); i++) {
    actual_checksum += header_loc[i];
  }

  result = actual_checksum == expected_checksum;
  free(copied_header);

  return result;
}

void free_entry(Entry *entry) {

  if (entry != NULL) {
    free(entry->header);
    free(entry);
  }
}

/* This function reads an entry into the reader's entry attribute and frees the
 * previous entry if needed. It will not automatically skip a files content, and
 * thus files can be processed using translate or skipped using skip functions
 * respectively.*/
int reader_cycle_entry(Reader *reader) {
  int bytes_read;
  TarHeader temp_header;
  Entry *new_entry = malloc(sizeof(Entry));
  TarHeader *new_header;

  if (new_entry == NULL) {
    fprintf(stderr, "Failed to allocate memory for new_entry.");
    exit(EXIT_FAILURE);
  }

  bytes_read = read(reader->src_fd, &temp_header, sizeof(TarHeader));

  if (bytes_read == -1) {
    free(new_entry);
    perror("Failed to read tar file when populating header.");
    exit(EXIT_FAILURE);
  }

  if (is_end_of_archive(&temp_header)) {
    free_entry(reader->current_entry);
    free(new_entry);
    return 0;
  }

  if (!is_valid_checksum(&temp_header)) {
    free(new_entry);
    fprintf(stderr, "Failed checksum. Exiting.\n");
    exit(EXIT_FAILURE);
  }

  new_header = malloc(sizeof(TarHeader));
  if (new_header == NULL) {
    free(new_entry);
    fprintf(stderr, "Failed to allocate memory for new_header.");
    exit(EXIT_FAILURE);
  }

  memcpy(new_header, &temp_header, sizeof(TarHeader));

  if (reader->is_strict) {
    if (strcmp((char *)new_header->magic, "ustar") != 0 ||
        new_header->version[0] != '0' || new_header->version[1] != '0') {
      free(new_entry);
      free(new_header);
      return -1;
    }
  }

  free_entry(reader->current_entry);

  new_entry->header = new_header;
  reader->current_entry = new_entry;
  return 1;
}
