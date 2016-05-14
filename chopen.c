#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <dirent.h> 
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

// #define DEBUG 1

#ifdef DEBUG
#define debug_print(fmt, ...) printf("%s:%d: " fmt, __FILE__, __LINE__, __VA_ARGS__);
#else
#define debug_print(fmt, ...) (void)0;
#endif

#define rename_failed(filename, fmt, ...) printf("Error (%s:%d): " \
                                          fmt, __FILE__, __LINE__, __VA_ARGS__); \
                                          return orig_unlink(filename);

/* CHANGE_MY_NAME varible is expected to be set to:
 *                old_name:new_name 
 */

const char* chopen_trigger_key    = "CHANGE_MY_NAME";
const char* chopen_trigger_value  = 0; /* these inits are mostly     */
const char* chopen_new_pname      = 0; /* redundant, but do not hurt */
size_t      chopen_old_pname_size = 0;
char        chopen_new_rname[PATH_MAX];

typedef int (*orig_open_type)(const char *pathname, int flags, ...);
orig_open_type orig_open;
orig_open_type orig_open64;

void _init(void) {
    // debug_print("in _init(), pid=%d\n", getpid());
    orig_open   = (orig_open_type)dlsym(RTLD_NEXT, "open");
    orig_open64 = (orig_open_type)dlsym(RTLD_NEXT, "open64");

    chopen_trigger_value = getenv(chopen_trigger_key);

    if (chopen_trigger_value) {
      for (chopen_new_pname = chopen_trigger_value; 
          *chopen_new_pname && *chopen_new_pname != ':'; chopen_new_pname++) 
      {
        /* */
      }
      chopen_old_pname_size = chopen_new_pname - chopen_trigger_value;
      chopen_new_pname = *chopen_new_pname == ':' ? ++chopen_new_pname : 0;
    }    
}

int open_priv(orig_open_type real_open, const char *pathname, int flags, mode_t mode)
{
  // debug_print("in open_priv: %s\n", pathname);

  if (chopen_new_pname) {
    size_t pathname_size = strlen(pathname);
    if (pathname_size > chopen_old_pname_size && 
        0 == strncmp(pathname + pathname_size - chopen_old_pname_size,
             chopen_trigger_value, chopen_old_pname_size) ) {

      size_t nbytes = pathname_size - chopen_old_pname_size;
      strncpy(chopen_new_rname, pathname, nbytes);

      strncpy(chopen_new_rname + nbytes, chopen_new_pname, PATH_MAX - nbytes -1);

      debug_print("chopen: tv=[%s] rp=[%s] np=[%s]\n", chopen_trigger_value,
                                                       pathname,
                                                       chopen_new_rname);
      pathname = chopen_new_rname;
    }
  }

  return real_open(pathname, flags, mode);
}

int open(const char * pathname, int flags, mode_t mode)
{
  return open_priv(orig_open, pathname, flags, mode);
}

int open64(const char * pathname, int flags, mode_t mode)
{
  return open_priv(orig_open64, pathname, flags, mode);
}

