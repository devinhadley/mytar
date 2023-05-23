/* mytar.c
 * This file includes functions for creating, listing, and extracting tar files.
 * It also manages the command line interface, including populating the flag
 * struct.
 */

#include "mytar.h"
#include "header.h"
#include "reader.h"
#include "writer.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

extern int getopt(int argc, char *const argv[], const char *optstring);
extern char *optarg;
extern int optind;
extern char *strdup(const char *);
extern int snprintf(char *str, size_t size, const char *format, ...);
extern int symlink(const char *target, const char *linkpath);

void init_flags(Flags *flags) {
  flags->create = false;
  flags->list = false;
  flags->extract = false;
  flags->verbose = false;
  flags->strict = false;
  flags->tarfile = NULL;
  flags->paths = NULL;
  flags->n_paths = 0;
}

/* Processes a file or directory and writes it into a tar file */
void process_path(const char *src, Writer *writer, bool is_verbose) {

  writer->header = init_header();

  populate_header_from_file(src, writer->header);

  writer_write_header(writer);

  switch (writer->header->typeflag) {
  case '0':
    writer_write_file(writer);
    writer_flush(writer);
    close(writer->src_fd);
    break;
  case '2':
    close(writer->src_fd);
    break;
  case '5':
  default:
    break;
  }

  if (is_verbose) {
    printf("%s\n", src);
  }

  free(writer->header);
}

/* Performes dfs on directory and its directories until all files are read. */
void traverse_path(const char *path, Writer *writer, bool is_verbose) {
  DIR *dir;
  struct dirent *entry;
  struct stat entry_stat;
  struct stat path_stat;
  char pathBuff[PATH_MAX];
  int path_len;

  path_len = strlen(path);
  strcpy(pathBuff, path);

  if (stat(path, &path_stat) != 0) {
    fprintf(stderr, "Cannot stat path %s\n", path);
    return;
  }

  /* append a slash if its a directory and doesnt already have slash */
  if (path_len == 0 ||
      (S_ISDIR(path_stat.st_mode) && path[path_len - 1] != '/'))
    strcat(pathBuff, "/");

  /* if the given path is a file or link */

  if (S_ISREG(path_stat.st_mode) || S_ISLNK(path_stat.st_mode)) {

    if ((writer->src_fd = open(path, O_RDONLY)) == -1) {
      perror("Failed to open source file: \n");
      printf("%s", path);
      exit(EXIT_FAILURE);
    }

    process_path(pathBuff, writer, is_verbose);

    return;
  }

  if ((dir = opendir(path)) == NULL) {
    perror("Failed to open dir.");
    exit(EXIT_FAILURE);
  }

  /* must process dir before opening it */
  process_path(pathBuff, writer, is_verbose);

  while ((entry = readdir(dir)) != NULL) {
    sprintf(pathBuff + path_len + (path[path_len - 1] != '/'), "%s",
            entry->d_name);

    if (stat(pathBuff, &entry_stat) != 0) {
      fprintf(stderr, "Cannot stat file %s\n", pathBuff);
      continue;
    }

    if (S_ISDIR(entry_stat.st_mode)) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;

      /* Directories dont need src fd because we are just parsing meta data.
       */

      strcat(pathBuff, "/");
      traverse_path(pathBuff, writer, is_verbose);

    } else {
      /* If handling a file, we need to change the source file descriptor
       * beforehand.*/

      if ((writer->src_fd = open(pathBuff, O_RDONLY)) == -1) {
        perror("Failed to open source file: \n");
        printf("%s: ", pathBuff);
        continue;
      }

      process_path(pathBuff, writer, is_verbose);
    }
  }

  closedir(dir);
}

/* Lists an archive entry with extra information include permissions, time,
 * etc.*/
void print_name_verbose(TarHeader *header, char *full_name) {
  char permissions[11];
  char owner[65];
  long size = 0;
  char mtime[17];
  time_t time_value;
  struct tm *time_info;

  permissions_to_string((char *)header->mode, permissions, header);

  snprintf(owner, sizeof(owner), "%s/%s", header->uname, header->gname);

  /* format size */
  size = strtol((char *)header->size, NULL, OCTAL_SIZE);

  /* format time */
  time_value = strtol((char *)header->mtime, NULL, OCTAL_SIZE);
  time_info = localtime(&time_value);
  strftime(mtime, sizeof(mtime), "%Y-%m-%d %H:%M", time_info);

  printf("%-10s %-17s %8ld %-16s %s\n", permissions, owner, size, mtime,
         full_name);
}

void print_entry(Flags *flags, Reader *reader, char *name) {
  if (flags->verbose) {
    print_name_verbose(reader->current_entry->header, name);
  } else {
    printf("%s\n", name);
  }

  /* if this is a file, not a dir. Skip the file contents */
  if (name[strlen(name) - 1] != '/') {
    reader_skip_file_contents(reader);
  }
}

/* This function takes a path, and will guarentee the path will exist in the
 * filesysem. If the path points to a file, it will return a file descriptor of
 * the file opened. etc.*/
int path_to_filesystem(const char *path, TarHeader *header) {
  char opath[PATH_MAX];
  char *p;
  size_t len;
  mode_t mode;
  int fd;
  char link_name[PATH_MAX];

  int stat_res;
  struct stat path_stat;

  strncpy(opath, path, sizeof(opath));
  opath[sizeof(opath) - 1] = '\0';
  len = strlen(opath);

  if (len == 0)
    return -1;

  for (p = opath; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      stat_res = stat(opath, &path_stat);
      if (stat_res != 0) {
        if (mkdir(opath, RWX_ALL) != 0) {
          return -1;
        }
      }
      *p = '/';
    }
  }
  if (opath[len - 1] != '/') {

    /* is this a file or a synmlink */

    if (header->typeflag == '2') {
      /* this is a symlink. */
      strncpy(link_name, (char *)header->linkname, sizeof(link_name));
      if (symlink(link_name, opath) == -1) {
        fprintf(stderr, "Failed to create symlink.\n");
      };
      return 0;
    }

    mode = strtol((char *)header->mode, NULL, OCTAL_SIZE);

    if (mode & S_IXUSR || mode & S_IXGRP || mode & S_IXOTH) {
      /* Grant execute permission to all if anyone has perms */
      mode = RWX_ALL;
    } else {
      mode = RW_ALL;
    }

    fd = open(opath, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd == -1) {
      perror("Failed to create/open when converting path to filesystem.\n");
      exit(EXIT_FAILURE);
    }
    return fd;
  } else if (access(opath, F_OK) != 0) {
    if (mkdir(opath, RWX_ALL) != 0) {
      return -1;
    }
  }
  return 0;
}

/* This function handels extracting a path, and writing to it in the filesystem.
 */
void extract_path(Flags *flags, Reader *reader, char *name) {

  Entry *entry = reader->current_entry;

  /* If this is strict dont extract header with special int */
  if (flags->strict) {
    if ((unsigned char)entry->header->uid[0] & 0x80) {
      fprintf(stderr, "Uid not strictly compliant. Skipping: %s", name);
      return;
    }
  }

  switch (entry->header->typeflag) {
    /* file */
  case '0':
  case '\0':

    reader->dst_fd = path_to_filesystem(name, reader->current_entry->header);

    reader_translate_to_file(reader);
    close(reader->dst_fd);

    if (flags->verbose) {
      printf("%s\n", name);
    }

    break;
    /* dir or symlink */
  case '5':
  case '2':
    path_to_filesystem(name, reader->current_entry->header);
    if (flags->verbose) {
      printf("%s\n", name);
    }
    break;
  }
}

/* This function will traverse the archive, and will execute the function
 * process_entry on any desired archive entries.
 */
void traverse_execute_archive(Reader *reader, char *archive_path, Flags *flags,
                              void (*process_entry)(Flags *, Reader *,
                                                    char *)) {
  int i;
  int fd;
  int prefix_len = 0;
  /* 257 to include normalizing / if need be */
  char path[PATH_MAX];
  int reader_status;
  char *prefix;
  bool match_found = false;

  /* treat every path as prefix;
   *Print if the prefix matches and the next is either null or /.
   *
   * */

  if ((fd = open(archive_path, O_RDONLY)) == -1) {
    fprintf(stderr, "Could not open archive when attempting to list.");
    exit(EXIT_FAILURE);
  }

  if (flags->n_paths != 0) {

    while ((reader_status = reader_cycle_entry(reader)) != 0) {

      if (reader_status == -1) {
        fprintf(stderr, "Encountered non-compliant entry. Skipping.\n");
        continue;
      }

      match_found = false;
      memset(path, 0, sizeof(path));
      extract_name(reader->current_entry->header, path);

      for (i = 0; i < flags->n_paths; i++) {

        prefix_len = strlen(flags->paths[i]);
        prefix = flags->paths[i];

        if (prefix[prefix_len - 1] == '/') {
          prefix_len -= 1;
        }

        /* the prefix matches */
        if (strncmp(path, prefix, prefix_len) == 0) {
          /* Is this an exact match, or a directory */

          if (prefix_len == strlen(path) || path[prefix_len] == '/') {
            process_entry(flags, reader, path);
            match_found = true;
            break;
          }
        }
      }
      /* if this path was a file and no entries found, skip its contents */
      if (!match_found && path[strlen(path) - 1] != '/') {
        reader_skip_file_contents(reader);
      }
    }
  } else {
    /* print all entries */
    while ((reader_status = reader_cycle_entry(reader)) != 0) {

      if (reader_status == -1) {
        fprintf(stderr, "Encountered non-compliant entry. Skipping.\n");
        continue;
      }

      memset(path, 0, sizeof(path));
      extract_name(reader->current_entry->header, path);

      process_entry(flags, reader, path);
    }
  }
}

void list_archive(Flags *flags) {

  Reader reader;
  reader_init(&reader, flags->strict);

  if ((reader.src_fd = open(flags->tarfile, O_RDONLY)) == -1) {
    perror("Could not open archive when attempting to list.");
    exit(EXIT_FAILURE);
  }

  traverse_execute_archive(&reader, flags->tarfile, flags, print_entry);
}
void extract_archive(Flags *flags) {

  Reader reader;
  reader_init(&reader, flags->strict);

  if ((reader.src_fd = open(flags->tarfile, O_RDONLY)) == -1) {
    perror("Could not open archive when attempting to list.");
    exit(EXIT_FAILURE);
  }

  traverse_execute_archive(&reader, flags->tarfile, flags, extract_path);
}

int main(int argc, char *argv[]) {
  Flags flags;
  Writer writer;
  int i;
  init_flags(&flags);
  writer_init(&writer);

  if (argc < 3) {
    fprintf(stderr, "usage: mytar [ctxvS]f tarfile [ path [ ... ] ]");
    exit(EXIT_FAILURE);
  }

  /* set any flags, and populate the paths */
  for (i = 0; i < strlen(argv[1]); i++) {
    switch (argv[1][i]) {
    case 'c':
      flags.create = true;
      break;
    case 't':
      flags.list = true;
      break;
    case 'x':
      flags.extract = true;
      break;
    case 'v':
      flags.verbose = true;
      break;
    case 'f':
      flags.tarfile = argv[2];
      break;
    case 'S':
      flags.strict = true;
      break;
    default:
      fprintf(stderr, "usage: mytar [ctxvS]f tarfile [ path [ ... ] ]");
      exit(EXIT_FAILURE);
    }
  }

  /* f is required */
  if (flags.tarfile == NULL) {
    fprintf(stderr, "usage: mytar [ctxvS]f tarfile [ path [ ... ] ]");
    exit(EXIT_FAILURE);
  }

  if (argc > 3) {
    flags.n_paths = argc - 3;
    flags.paths = &argv[3];
  }

  if (flags.list) {
    list_archive(&flags);
    return 0;
  }

  if (flags.create) {

    if ((writer.dst_fd =
             open(flags.tarfile, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1) {
      perror("Failed to open destination file");
      exit(EXIT_FAILURE);
    }
    for (i = 0; i < flags.n_paths; i++) {
      traverse_path(flags.paths[i], &writer, flags.verbose);
    }

    writer_pad(&writer);
    writer_flush(&writer);
    close(writer.dst_fd);
    return 0;
  }

  if (flags.extract) {
    extract_archive(&flags);
    return 0;
  }

  return 0;
}
