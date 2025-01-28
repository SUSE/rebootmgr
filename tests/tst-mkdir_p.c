
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ftw.h>

#include "basics.h"

#include "common.h"

/* test that mkdir_p works */

#define TEST_PATH "tests/a/b/c/d/9/8/7/e"

static int
rm(const char *path, const struct stat _unused_(*sbuf),
   int _unused_(type), struct FTW _unused_(*ftwb))
{
  if(remove(path) < 0)
    {
      fprintf(stderr, "rm: remove failed: %m\n");
      return -1;
    }
  return 0;
}

static int
delete_test_path(void)
{
  struct stat st;

  if (stat("tests/a", &st) < 0)
    return 0;

  /* Delete the directory and its contents by traversing the tree in
     reverse order, without crossing mount boundaries and symbolic links */
  if (nftw("tests/a", rm, 12, FTW_DEPTH|FTW_MOUNT|FTW_PHYS) < 0)
    {
      fprintf (stderr, "nftw() failed: %m\n");
      return -1;
    }

  return 0;
}

int
main(void)
{
  struct stat st;
  int r;

  /* make sure the path does not exist */
  r = delete_test_path();
  if (r < 0)
    return 1;

  r = mkdir_p(TEST_PATH, 0755);
  if (r < 0)
    {
      fprintf(stderr, "mkdir_p failed: %s\n", strerror(-r));
      return 1;
    }

  if (stat(TEST_PATH, &st) < 0)
    {
      fprintf(stderr, "stat failed: %m\n");
      return 1;
    }

  /* cleanup after us */
  delete_test_path();

  return 0;
}
