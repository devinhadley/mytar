/* header.c
 * This file is in charge of handling creation and field extraction tasks
 * related to tarheaders. These tasks include the generation of header structs
 * and the extraction of specific struct attributes.
 * */
#include "header.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <grp.h>
#include <linux/limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern int lstat(const char *file, struct stat *buf);
extern ssize_t readlink(const char *pathname, char *buf, size_t bufsiz);
extern int snprintf(char *str, size_t size, const char *format, ...);

/* allocates a new header and initializes all values to 0 */
TarHeader *init_header() {

  TarHeader *header = malloc(sizeof(TarHeader));

  memset(header, 0, sizeof(TarHeader));

  return header;
}

/* Converts a header's permission representation from octal to rwx format. */
void permissions_to_string(char *octal_str, char *str, TarHeader *header) {
  int i;
  int octal;

  if (str == NULL || octal_str == NULL)
    return;

  switch (header->typeflag) {
  case '5':
    str[0] = 'd';
    break;
  case '2':
    str[0] = 'l';
    break;
  default:
    str[0] = '-';
    break;
  }

  octal = strtol(octal_str, NULL, 8);

  for (i = 2; i >= 0; --i) {
    str[i * 3 + 1] = (octal & 04) ? 'r' : '-';
    str[i * 3 + 2] = (octal & 02) ? 'w' : '-';
    str[i * 3 + 3] = (octal & 01) ? 'x' : '-';
    octal >>= 3;
  }
  str[10] = '\0';
}

void print_tar_header(const TarHeader *header) {
  printf("Name: %.*s\n", (int)sizeof(header->name), header->name);
  printf("Mode: %.*s\n", (int)sizeof(header->mode), header->mode);
  printf("UID: %.*s\n", (int)sizeof(header->uid), header->uid);
  printf("GID: %.*s\n", (int)sizeof(header->gid), header->gid);
  printf("Size: %.*s\n", (int)sizeof(header->size), header->size);
  printf("MTime: %.*s\n", (int)sizeof(header->mtime), header->mtime);
  printf("Checksum: %.*s\n", (int)sizeof(header->chksum), header->chksum);
  printf("Typeflag: %c\n", header->typeflag);
  printf("Linkname: %.*s\n", (int)sizeof(header->linkname), header->linkname);
  printf("Magic: %.*s\n", (int)sizeof(header->magic), header->magic);
  printf("Version: %.*s\n", (int)sizeof(header->version), header->version);
  printf("Uname: %.*s\n", (int)sizeof(header->uname), header->uname);
  printf("Gname: %.*s\n", (int)sizeof(header->gname), header->gname);
  printf("Devmajor: %.*s\n", (int)sizeof(header->devmajor), header->devmajor);
  printf("Devminor: %.*s\n", (int)sizeof(header->devminor), header->devminor);
  printf("Prefix: %.*s\n", (int)sizeof(header->prefix), header->prefix);
  printf("Unused: %.*s\n", (int)sizeof(header->unused), header->unused);
}

/* Properly extracts the path of a file given the header */
char *extract_name(TarHeader *header, char *full_name) {
  char temp_prefix[sizeof(header->prefix) + 1];
  char temp_name[sizeof(header->name) + 1];

  snprintf(temp_prefix, sizeof(temp_prefix), "%s", header->prefix);
  snprintf(temp_name, sizeof(temp_name), "%s", header->name);

  if (temp_prefix[0] != '\0') {
    snprintf(full_name, sizeof(header->prefix) + sizeof(header->name) + 2,
             "%s/%s", temp_prefix, temp_name);
  } else {
    snprintf(full_name, sizeof(header->name) + 1, "%s", temp_name);
  }

  return full_name;
}

/* Properly inserts a path into a tarheader by properly inserting into prefix
 * and name in accordance with the ustar standard
 */
void populate_name(const char *path, struct stat *path_stat,
                   TarHeader *header) {

  int i;

  if (strlen(path) > sizeof(header->name)) {
    for (i = strlen(path) - sizeof(header->name) - 1; i >= 0; i++) {
      if (path[i] == '/') {
        break;
      }
    }
    memcpy(header->name, path + i + 1, strlen(path) - i - 1);
    memcpy(header->prefix, path, i);
  } else {
    memcpy(header->name, path, strlen(path));
  }
}

/* Calculates and inserts the checksum into the tarheader */
void populate_chksum(TarHeader *header) {

  unsigned char *header_loc = (unsigned char *)header;
  int i;
  int sum = 0;

  memset(header->chksum, ' ', sizeof(header->chksum));

  for (i = 0; i < sizeof(TarHeader); i++) {
    sum += header_loc[i];
  }

  sprintf((char *)header->chksum, "%07o", sum);
}

/* This function populates the typeflag of the header, and the linkname if its a
 * link */
void populate_type_linkname(const char *path, struct stat *path_stat,
                            TarHeader *header) {

  char buffer[PATH_MAX];
  int len;
  if (S_ISLNK(path_stat->st_mode)) {

    header->typeflag = '2';

    len = readlink(path, buffer, sizeof(buffer));

    if (len == -1) {
      perror("Failed to read link: ");
      exit(EXIT_FAILURE);
    }

    if (len > 100) {
      fprintf(stderr, "linkname greater than 100:\n%s\n", path);
      exit(EXIT_FAILURE);
    }

    strcpy((char *)header->linkname, buffer);

  } else if (S_ISDIR(path_stat->st_mode)) {

    header->typeflag = '5';

  } else {
    header->typeflag = '0';
  }
}

void populate_uname_gname(struct stat *path_stat, TarHeader *header) {
  struct passwd *owner_info = getpwuid(path_stat->st_uid);
  struct group *group_info = getgrgid(path_stat->st_gid);

  if (owner_info == NULL) {
    fprintf(stderr, "Failed to get file owner info");
    exit(EXIT_FAILURE);
  }

  if (group_info == NULL) {
    fprintf(stderr, "Failed to get group info");
    exit(EXIT_FAILURE);
  }

  strncpy((char *)header->uname, owner_info->pw_name,
          sizeof(header->uname) - 1);

  strncpy((char *)header->gname, group_info->gr_name,
          sizeof(header->gname) - 1);
}

int insert_special_int(char *where, size_t size, int32_t val) {
  /* For interoperability with GNU tar. GNU seems to
   * set the high–order bit of the first byte, then
   * treat the rest of the field as a binary integer
   * in network byte order.
   * Insert the given integer into the given field
   * using this technique. Returns 0 on success, nonzero
   * otherwise
   */
  int err = 0;
  if (val < 0 || (size < sizeof(val))) {
    /* if it’s negative, bit 31 is set and we can’t use the flag
     * if len is too small, we can’t write it. Either way, we’re
     * done.
     */
    err++;
  } else {
    /* game on....*/
    memset(where, 0, size); /* Clear out the buffer */
    *(int32_t *)(where + size - sizeof(val)) = htonl(val); /* place the int */
    *where |= 0x80; /* set that high–order bit */
  }
  return err;
}

/* populates the uid and gid, if uid and gid are too big then special int is
 * inserted */
void populate_uid_gid(struct stat *path_stat, TarHeader *header) {

  if (path_stat->st_uid > 07777777) {

    insert_special_int((char *)header->uid, sizeof(header->uid),
                       path_stat->st_uid);

  } else {
    sprintf((char *)header->uid, "%07o", path_stat->st_uid);
  }

  if (path_stat->st_gid > 07777777) {

    insert_special_int((char *)header->gid, sizeof(header->gid),
                       path_stat->st_gid);

  } else {
    sprintf((char *)header->gid, "%07o", path_stat->st_gid);
  }
}

void populate_size(struct stat *path_stat, TarHeader *header) {
  if (S_ISDIR(path_stat->st_mode) || S_ISLNK(path_stat->st_mode)) {
    sprintf((char *)header->size, "%011lo", 0l);
    return;
  }

  sprintf((char *)header->size, "%011lo", path_stat->st_size);
}

/* Populates a tar header given a path to a file */
void populate_header_from_file(const char *path, TarHeader *header) {

  struct stat path_stat;

  if (lstat(path, &path_stat) == -1) {
    printf("%s\n", path);
    perror("Error stating when populating header.");
    exit(EXIT_FAILURE);
  }

  populate_name(path, &path_stat, header);

  /* populate mode */
  sprintf((char *)header->mode, "%07o", path_stat.st_mode & 07777);

  /* populate uid and gid */
  populate_uid_gid(&path_stat, header);

  /* populate size */
  populate_size(&path_stat, header);

  /* populate mtime */
  sprintf((char *)header->mtime, "%011lo", path_stat.st_mtime);

  /* populate typeflag and linkname if need be */
  populate_type_linkname(path, &path_stat, header);

  /* populate magic */
  strcpy((char *)header->magic, "ustar");

  /* populate version */
  strcpy((char *)header->version, "00");

  /* populate uname, gname */
  populate_uname_gname(&path_stat, header);

  /* devmajor devminor remain NULL */

  /* populate checksum */
  populate_chksum(header);
}
