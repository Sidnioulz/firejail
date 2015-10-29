/* 
    2015 (c) Steve Dodier-Lazaro <sidnioulz@gmail.com>
    This file is part of ExecHelper.

    ExecHelper is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    ExecHelper is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with ExecHelper.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _GNU_SOURCE

#include "../include/exechelper.h"
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>


char *exechelp_resolve_path(const char *file)
{
  if (!file || *file == '\0')
    return NULL;

  /* Don't search when it contains a slash.  */
  if (strchr(file, '/') != NULL) {
    char *real = realpath(file, NULL);
    return real;
  }
  else {
    size_t pathlen;
    size_t alloclen = 0;
    char *path = getenv("PATH");
    if (path == NULL) {
      pathlen = confstr(_CS_PATH, (char *) NULL, 0);
      alloclen = pathlen + 1;
    }
    else
      pathlen = strlen(path);

    size_t len = strlen(file) + 1;
    alloclen += pathlen + len + 1;

    char *name;
    char *path_malloc = NULL;
    path_malloc = name = malloc(alloclen);
    if (name == NULL)
      return NULL;

    if (path == NULL) {
      /* There is no `PATH' in the environment.
         The default search path is the current directory
         followed by the path `confstr' returns for `_CS_PATH'.  */
      path = name + pathlen + len + 1;
      path[0] = ':';
      (void) confstr(_CS_PATH, path + 1, pathlen);
    }

    /* Copy the file name at the top.  */
    name = (char *) memcpy(name + pathlen + 1, file, len);
    /* And add the slash.  */
    *--name = '/';

    char *return_value = NULL;
    short succeeded = 0;
    short got_eacces = 0;
    char *p = path;
    do {
      char *startp;

      path = p;
      p = strchrnul(path, ':');

      if (p == path)
        /* Two adjacent colons, or a colon at the beginning or the end
           of `PATH' means to search the current directory.  */
        startp = name + 1;
      else
        startp = (char *) memcpy(name - (p - path), path, p - path);
      
      /* Try to execute this name.  If it works, execve will not return. */
      errno = 0;
      if (access(startp, X_OK)) {
        switch (errno) {
          case EACCES:
            /* Record the we got a `Permission denied' error.  If we end
               up finding no executable we can use, we want to diagnose
               that we did find one but were denied access.  */
            got_eacces = 1;
          case ENOENT:
          case ENAMETOOLONG:
          case ENOTDIR:
          case ELOOP:
            /* Those errors indicate the file is missing or not executable
               by us, in which case we want to just try the next path
               directory.  */
          case EROFS:
          case ETXTBSY:
            /* Some strange filesystems like AFS return even
               stranger error numbers.  They cannot reasonably mean
               anything else so ignore those, too.  */
            break;

          default:
            /* Some other error means we found an executable file, but
               something went wrong executing it; return the error to our
               caller.  */
            return NULL;
        }
      }
      else {
        succeeded = 1;
        return_value = strdup(startp);
      }
    }
    while (*p++ != '\0' && !succeeded);

    /* At least one failure was due to permissions, so report that
       error.  */
    if (got_eacces)
      errno = EACCES;

    free (path_malloc);

    return return_value;
  }
}

#define APPLINKS_DIR "/etc/firejail/applinks.d"

ExecHelpBinaryAssociations *exechelp_get_binary_associations()
{
  static ExecHelpBinaryAssociations *assoc = NULL;

  if(!assoc) {
    assoc = malloc (sizeof(ExecHelpBinaryAssociations));
    assoc->assoc = NULL;
    assoc->index = exechelp_hash_table_new(exechelp_str_hash, exechelp_str_equal);

    // timing this process for informational purposes...
    time_t t1, t2;
    t1 = time(0);
    size_t count=0;

    DIR* FD;
    if (NULL == (FD = opendir(APPLINKS_DIR))) {
      fprintf(stderr, "Error: could not initialise linked apps because '"APPLINKS_DIR"' could not be opened: %s\n", strerror(errno));
      exechelp_hash_table_destroy(assoc->index);
      free(assoc);
      return NULL;
    }

    struct dirent* package_dir;
    while ((package_dir = readdir(FD))) {
        ++count;
        if (!strcmp(package_dir->d_name, ".") || !strcmp(package_dir->d_name, ".."))
            continue;

        char *mainpath, *linkpath, *mainbinary;
        if (asprintf(&mainpath, APPLINKS_DIR"/%s/main", package_dir->d_name) == -1) {
          fprintf(stderr, "Error: could not initialise linked apps of a memory allocation error (asprintf main path): %s\n", strerror(errno));
          exechelp_hash_table_destroy(assoc->index);
          //TODO: de-allocate inside assoc->index
          //TODO: free assoc->assoc
          free(assoc);
          return NULL;
        }
        FILE* mainfile = fopen(mainpath, "rb");
        free(mainpath);
        if (!mainfile) {
          DEBUG("Error: package '%s' does not have a main binary, yet is present in the applinks.d directory, ignoring package\n", package_dir->d_name);
          continue;
        }

        mainbinary = NULL;
        size_t n = 0;
        ssize_t linelen = getline(&mainbinary, &n, mainfile);
        fclose(mainfile);
        if (linelen == -1) {
          DEBUG("Error: could not read the main binary for package '%s', ignoring package\n", package_dir->d_name);
          free(mainbinary);
          continue;
        }

        if (mainbinary[linelen-1] == '\n')
          mainbinary[linelen-1] = '\0';

        if (asprintf(&linkpath, APPLINKS_DIR"/%s/linked", package_dir->d_name) == -1) {
          fprintf(stderr, "Error: could not initialise linked apps of a memory allocation error (asprintf linked path): %s\n", strerror(errno));
          exechelp_hash_table_destroy(assoc->index);
          //TODO: de-allocate inside assoc->index
          //TODO: free assoc->assoc
          free(assoc);
          return NULL;
        }
        FILE* linkfile = fopen(linkpath, "rb");
        free(linkpath);
        if (!linkfile) {
          DEBUG("Error: package '%s' does not have a list of linked binaries, yet is present in the applinks.d directory, ignoring package\n", package_dir->d_name);
          free(mainbinary);
          continue;
        }
        
        ExecHelpSList *list = NULL;
        char *buffer = NULL;
        n = 0;
        while ((linelen = getline(&buffer, &n, linkfile)) != -1) {
          if (buffer[linelen-1] == '\n')
            buffer[linelen-1] = '\0';

          char *currentlink = buffer;
          buffer = NULL;
          n = 0;
          list = exechelp_slist_prepend(list, currentlink);
          exechelp_hash_table_insert(assoc->index, currentlink, mainbinary);
        }
        list = exechelp_slist_prepend(list, mainbinary);
        assoc->assoc = exechelp_slist_prepend(assoc->assoc, list);
        fclose(linkfile);
    }

    t2 = time(0);
    DEBUG("It took %f seconds to load %lu application link definitions.\n", difftime(t2, t1), count);
  }

  return assoc;
}

ExecHelpSList *exechelp_get_associations_for_main_binary(ExecHelpBinaryAssociations *assoc, const char *mainkey)
{
  if(!assoc || !mainkey)
    return NULL;

  ExecHelpSList *assocs = assoc->assoc;
  while (assocs) {
   // printf ("?%p %s \n ", assocs->data, assocs->data? ((ExecHelpSList *)(assocs->data))->data : "niiil");
    if (assocs->data && (strcmp(((ExecHelpSList *)(assocs->data))->data, mainkey) == 0))
      break;

    assocs = assocs->next;
  }
  if (assocs)
    assocs = assocs->data;

  return assocs;
}

ExecHelpSList *exechelp_get_associations_for_arbitrary_binary(ExecHelpBinaryAssociations *assoc, const char *key)
{
  if (!key)
    return NULL;

  ExecHelpSList *ret  = NULL;
  char          *path = NULL;
  
  if (!exechelp_hash_table_contains(assoc->index, key)) {
    DEBUG2("DEBUG: binary '%s' is not a full path, calling realpath()\n", key);
    path = exechelp_resolve_path(key);
    if (!path) {
      DEBUG2("DEBUG: no binary for the name '%s' could be found, aborting\n", key);
      return NULL;
    }
    else
      DEBUG2("DEBUG: name '%s' corresponds to binary '%s'\n", key, path);
  }
  
  if (exechelp_hash_table_contains(assoc->index, path?path:key)) {
    DEBUG2("DEBUG: binary '%s' has associations with other apps\n", path?path:key);
    const char *mainkey = exechelp_hash_table_lookup(assoc->index, path?path:key);

    if (mainkey) {
      DEBUG2("DEBUG: binary '%s' 's parent app is %s\n", path?path:key, mainkey);
      ret = exechelp_get_associations_for_main_binary(assoc, mainkey);
    }
    else
      DEBUG2("DEBUG: binary '%s' 's parent app could not be found\n", path?path:key);
  }
  else {
    DEBUG2("DEBUG: binary '%s' is not associated with other apps\n", path?path:key);
  }

  if (path)
    free(path);

  return ret;
}

int exechelp_is_associated_app(const char *caller, const char *callee)
{
  ExecHelpBinaryAssociations *assoc = exechelp_get_binary_associations();
  ExecHelpSList *assocs = exechelp_get_associations_for_arbitrary_binary(assoc, caller);

  int associated = 0;

  if (assocs)
    associated = exechelp_slist_find_custom(assocs, callee, (ExecHelpCompareFunc)strcmp) != NULL;

  DEBUG2 ("DEBUG: callee '%s' is %sin the list of associated apps for caller '%s'\n", callee, (associated? "":"not "), caller);
  return associated;
}

char *exechelp_extract_associations_for_binary(const char *receiving_binary)
{
  if (receiving_binary) {
    DEBUG2("DEBUG: extracting the binary associations for '%s'\n", receiving_binary);

    ExecHelpBinaryAssociations *assoc = exechelp_get_binary_associations();
    ExecHelpSList *assocs = exechelp_get_associations_for_arbitrary_binary(assoc, receiving_binary);

    if (assocs) {
      char *names = malloc(sizeof(char));
      names[0] = '\0';
      ExecHelpSList *iter = assocs;
      while(iter && names) {
        char *prev_names = names;
        char *data = iter->data;

        size_t new_len = strlen(names) + strlen(data) + 1 /* ":" */ + 1;
        names = malloc(sizeof(char) * new_len);
        if (names)
          snprintf(names, new_len, "%s%s%s", prev_names, (prev_names[0] != '\0' ? ":":""), data);

        free (prev_names);
        iter = iter->next;
      }

      return names;
    }
    else
      DEBUG2("DEBUG: %s", "receiving binary is not associated with other apps\n");
  }

  return NULL;
}

void *exechelp_malloc0(size_t size)
{
  void *mem = malloc(size);
  if (mem)
    memset(mem, 0, size);
  return mem;
}

void *exechelp_memdup (const void *mem, unsigned int byte_size)
{
  void *new_mem;

  if (mem) {
    new_mem = malloc(byte_size);
    memcpy(new_mem, mem, byte_size);
  }
  else
    new_mem = NULL;

  return new_mem;
}


#define CAN_MODE_MASK (CAN_EXISTING | CAN_ALL_BUT_LAST | CAN_MISSING)
#define AREAD_MAX_SIZE 4096
#define IS_ABSOLUTE_FILE_NAME(F) ISSLASH ((F)[0])
#define IS_RELATIVE_FILE_NAME(F) (! IS_ABSOLUTE_FILE_NAME (F))
#define MULTIPLE_BITS_SET(i) (((i) & ((i) - 1)) != 0)
#ifndef PATH_MAX
# define PATH_MAX 8192
#endif
# define ISSLASH(C) ((C) == '/')

enum _exechelp_canonicalize_mode_t {
    /* All components must exist.  */
    CAN_EXISTING = 0,

    /* All components excluding last one must exist.  */
    CAN_ALL_BUT_LAST = 1,

    /* No requirements on components existence.  */
    CAN_MISSING = 2,

    /* Don't expand symlinks.  */
    CAN_NOLINKS = 4
  };
typedef enum _exechelp_canonicalize_mode_t _exechelp_canonicalize_mode_t;

static char *_exechelp_canonicalize_filename_mode (const char *, _exechelp_canonicalize_mode_t);

char *exechelp_coreutils_areadlink_with_size (char const *file, size_t size)
{
  /* Some buggy file systems report garbage in st_size.  Defend
   * against them by ignoring outlandish st_size values in the initial
   * memory allocation.
   */
  size_t symlink_max = 1024;
  size_t INITIAL_LIMIT_BOUND = 8 * 1024;
  size_t initial_limit = (symlink_max < INITIAL_LIMIT_BOUND
                          ? symlink_max + 1
                          : INITIAL_LIMIT_BOUND);

  /* The initial buffer size for the link value.  */
  size_t buf_size = size < initial_limit ? size + 1 : initial_limit;

  while (1) {
    ssize_t r;
    size_t link_length;
    char *buffer = malloc (buf_size);

    if (buffer == NULL)
      return NULL;
    r = readlink (file, buffer, buf_size);
    link_length = r;

    /* On AIX 5L v5.3 and HP-UX 11i v2 04/09, readlink returns -1
     * with errno == ERANGE if the buffer is too small.
     */
    if (r < 0 && errno != ERANGE) {
      int saved_errno = errno;
      free (buffer);
      errno = saved_errno;
      return NULL;
    }

    if (link_length < buf_size) {
      buffer[link_length] = 0;
      return buffer;
    }

    free (buffer);
    if (buf_size <= AREAD_MAX_SIZE / 2)
      buf_size *= 2;
    else if (buf_size < AREAD_MAX_SIZE)
      buf_size = AREAD_MAX_SIZE;
    else {
      errno = ENOMEM;
      return NULL;
    }
  }
}

/* Return 1 if we've already seen the triple, <FILENAME, dev, ino>.
 * If *HT is not initialized, initialize it.
 */
static int _exechelp_seen_triple (ExecHelpHashTable **h, char const *filename, struct stat const *st)
{
  if (*h == NULL) {
    *h = exechelp_hash_table_new_full(exechelp_str_hash, exechelp_str_equal, free, free); 

    /* memory is full, nothing clever to do */
    if (*h == NULL)
      return 0;
  }

  struct stat *lookup = exechelp_hash_table_lookup(*h, filename);
  if (lookup) {
    if (lookup->st_ino == st->st_ino && lookup->st_dev == st->st_dev)
      return 1;
  }

  struct stat *record = malloc(sizeof(struct stat));
  record = memcpy(record, st, sizeof(struct stat));

  exechelp_hash_table_insert(*h, strdup(filename), record);
  
  return 0;
}

/* Return the canonical absolute name of file NAME, while treating
 * missing elements according to CAN_MODE.  A canonical name
 * does not contain any ".", ".." components nor any repeated file name
 * separators ('/') or, depending on other CAN_MODE flags, symlinks.
 * Whether components must exist or not depends on canonicalize mode.
 * The result is malloc'd.
 */
static char *_exechelp_canonicalize_filename_mode (const char *name, _exechelp_canonicalize_mode_t can_mode)
{
  char *rname, *dest, *extra_buf = NULL;
  char const *start;
  char const *end;
  char const *rname_limit;
  size_t extra_len = 0;
  ExecHelpHashTable *h = NULL;
  int saved_errno;
  int can_flags = can_mode & ~CAN_MODE_MASK;
  int logical = can_flags & CAN_NOLINKS;

  can_mode &= CAN_MODE_MASK;

  if (MULTIPLE_BITS_SET (can_mode)) {
    errno = EINVAL;
    return NULL;
  }

  if (name == NULL) {
    errno = EINVAL;
    return NULL;
  }

  if (name[0] == '\0') {
    errno = ENOENT;
    return NULL;
  }

  if (!IS_ABSOLUTE_FILE_NAME (name)) {
    if(name[0] == '~') {
      if (!ISSLASH(name[1])) {
        errno = EINVAL;
        return NULL;
      }
      
      rname = getenv ("HOME");
      if (!rname)
        return NULL;
      else
        rname = strdup(rname);
      if (!rname)
        return NULL;
      dest = strchr (rname, '\0');
      if (dest - rname < PATH_MAX) {
        char *p = realloc (rname, PATH_MAX);
        dest = p + (dest - rname);
        rname = p;
        rname_limit = rname + PATH_MAX;
      }
      else {
        rname_limit = dest;
      }
      start = name + 2; // '~/'
    }
    else {
      rname = getcwd (NULL, 0);
      if (!rname)
        return NULL;
      dest = strchr (rname, '\0');
      if (dest - rname < PATH_MAX) {
        char *p = realloc (rname, PATH_MAX);
        dest = p + (dest - rname);
        rname = p;
        rname_limit = rname + PATH_MAX;
      }
      else {
        rname_limit = dest;
      }
      start = name;
    }
  }
  else {
    rname = malloc (PATH_MAX);
    rname_limit = rname + PATH_MAX;
    dest = rname;
    *dest++ = '/';
    start = name;
  }

  for ( ; *start; start = end) {
    /* Skip sequence of multiple file name separators.  */
    while (ISSLASH (*start))
      ++start;

    /* Find end of component.  */
    for (end = start; *end && !ISSLASH (*end); ++end)
      /* Nothing.  */;

    if (end - start == 0)
      break;
    else if (end - start == 1 && start[0] == '.')
      /* nothing */;
    else if (end - start == 2 && start[0] == '.' && start[1] == '.') {
      /* Back up to previous component, ignore if at root already.  */
      if (dest > rname + 1)
        for (--dest; dest > rname && !ISSLASH (dest[-1]); --dest)
          continue;
    }
    else {
      struct stat st;

      if (!ISSLASH (dest[-1]))
        *dest++ = '/';

      if (dest + (end - start) >= rname_limit) {
        size_t dest_offset = dest - rname;
        size_t new_size = rname_limit - rname;

        if (end - start + 1 > PATH_MAX)
          new_size += end - start + 1;
        else
          new_size += PATH_MAX;
        rname = realloc (rname, new_size);
        rname_limit = rname + new_size;

        dest = rname + dest_offset;
      }

      dest = memcpy (dest, start, end - start);
      dest += end - start;
      *dest = '\0';

      if (logical && (can_mode == CAN_MISSING)) {
        /* Avoid the stat in this case as it's inconsequential.
         * i.e. we're neither resolving symlinks or testing
         * component existence.
         */
        st.st_mode = 0;
      }
      else if ((logical ? stat (rname, &st) : lstat (rname, &st)) != 0) {
        saved_errno = errno;
        if (can_mode == CAN_EXISTING)
          goto error;
        if (can_mode == CAN_ALL_BUT_LAST) {
          if (end[strspn (end, "/")] || saved_errno != ENOENT)
            goto error;
          continue;
        }
        st.st_mode = 0;
      }

      if (S_ISLNK (st.st_mode)) {
        char *buf;
        size_t n, len;

        /* Detect loops.  We cannot use the cycle-check module here,
         * since it's actually possible to encounter the same symlink
         * more than once in a given traversal.  However, encountering
         * the same symlink,NAME pair twice does indicate a loop.
         */
        if (_exechelp_seen_triple (&h, name, &st)) {
          if (can_mode == CAN_MISSING)
            continue;
          saved_errno = ELOOP;
          goto error;
        }

        buf = exechelp_coreutils_areadlink_with_size (rname, st.st_size);
        if (!buf) {
          if (can_mode == CAN_MISSING && errno != ENOMEM)
            continue;
          saved_errno = errno;
          goto error;
        }

        n = strlen (buf);
        len = strlen (end);

        if (!extra_len) {
          extra_len =
            ((n + len + 1) > PATH_MAX) ? (n + len + 1) : PATH_MAX;
          extra_buf = malloc (extra_len);
        }
        else if ((n + len + 1) > extra_len) {
          extra_len = n + len + 1;
          extra_buf = realloc (extra_buf, extra_len);
        }

        /* Careful here, end may be a pointer into extra_buf... */
        memmove (&extra_buf[n], end, len + 1);
        name = end = memcpy (extra_buf, buf, n);

        if (IS_ABSOLUTE_FILE_NAME (buf)) {
          dest = rname;
          *dest++ = '/'; /* It's an absolute symlink */
        }
        else {
          /* Back up to previous component, ignore if at root already: */
          if (dest > rname + 1)
            for (--dest; dest > rname && !ISSLASH (dest[-1]); --dest)
              continue;
        }

        free (buf);
      }
      else {
        if (!S_ISDIR (st.st_mode) && *end && (can_mode != CAN_MISSING)) {
          saved_errno = ENOTDIR;
          goto error;
        }
      }
    }
  }
  if (dest > rname + 1 && ISSLASH (dest[-1]))
    --dest;
  *dest = '\0';
  if (rname_limit != dest + 1)
    rname = realloc (rname, dest - rname + 1);

  free (extra_buf);
  if (h)
    exechelp_hash_table_destroy (h);
  return rname;

  error:
  free (extra_buf);
  free (rname);
  if (h)
    exechelp_hash_table_destroy (h);
  errno = saved_errno;
  return NULL;
}

char *exechelp_coreutils_realpath (const char *fname)
{
  int can_mode = CAN_MISSING;
  char *can_fname = _exechelp_canonicalize_filename_mode (fname, can_mode);
  if (can_fname)  /* canonicalize again to resolve symlinks.  */ {
    can_mode &= ~CAN_NOLINKS;
    char *can_fname2 = _exechelp_canonicalize_filename_mode (can_fname, can_mode);
    free (can_fname);
    can_fname = can_fname2;
  }
  
  if (!can_fname)
    DEBUG2("ERROR: '%s': %d\n", fname, errno);

  return can_fname;
}

