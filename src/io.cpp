/*
 * io.c
 *
 *  Created on: Jul 6, 2016
 *      Author: reboot
 */

#include "io.h"
#include "str.h"
#include "int_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

int
find_absolute_path (const char *exec, char *output)
{
  char *env = getenv ("PATH");

  if (!env)
    {
      return 1;
    }

  mda s_p;

  md_init (&s_p);

  int p_c = split_string (env, 0x3A, &s_p);

  if (p_c < 1)
    {
      return 2;
    }

  p_md_obj ptr = s_p.first;

  while (ptr)
    {
      snprintf (output, PATH_MAX, "%s/%s", (char*) ptr->ptr, exec);
      if (!access (output, R_OK | X_OK))
	{
	  md_free (&s_p);
	  return 0;
	}
      ptr = ptr->next;
    }

  md_free (&s_p);

  return 3;
}

int
self_get_path (const char *exec, char *out)
{

  char path[PATH_MAX];
  int r;

  snprintf (path, PATH_MAX, "/proc/%d/exe", getpid ());

  if (access (path, R_OK))
    {
      snprintf (path, PATH_MAX, "/compat/linux/proc/%d/exe", getpid ());
    }
  else
    {
      goto read;
    }

  if (access (path, R_OK))
    {
      snprintf (path, PATH_MAX, "/proc/%d/file", getpid ());
    }

  read:

  if ((r = readlink (path, out, PATH_MAX)) < 1)
    {
      if ((r = find_absolute_path (exec, out)))
	{
	  snprintf (out, PATH_MAX, "%s", exec);
	}
      return 0;
    }

  out[r] = 0x0;
  return 0;
}

int
dir_exists (char *dir)
{
  int r = get_file_type (dir);

  if (r == DT_DIR)
    {
      return 0;
    }
  return 1;
}

int
get_file_type (char *file)
{
  struct stat sb;

  if (stat (file, &sb) == -1)
    {
      return DT_UNKNOWN;
    }

  return IFTODT(sb.st_mode);
}
