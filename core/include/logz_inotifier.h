#ifndef _LOGZ_INOTIFIER_H_
#define _LOGZ_INOTIFIER_H_

#if HAVE_INOTIFY
# include "hash.h"
# include <sys/inotify.h>
# include <sys/select.h>
# include "fs.h"
# include "fs-is-local.h"
# if HAVE_SYS_STATFS_H
#  include <sys/statfs.h>
# elif HAVE_SYS_VFS_H
#  include <sys/vfs.h>
# endif
#endif

#include <stdbool.h>

struct logz_file_def;
struct FILE;

static inline bool
any_symlinks (
    const struct logz_file_def *filedef,
    size_t num_files) {

  size_t i;

  struct stat st;
  for (i = 0; i < num_files; i++)
    if (lstat (filedef[i].name, &st) == 0 && S_ISLNK (st.st_mode))
      return true;
  return false;
}

bool logz_recursive_inotify(int wd, struct logz_file_def *f, size_t n_files, FILE *target_stream);

#endif /* _LOGZ_INOTIFIER_H_ */
