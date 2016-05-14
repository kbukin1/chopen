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

//#define DEBUG 1

#ifdef DEBUG
#define debug_print(fmt, ...) printf("%s:%d: " fmt, __FILE__, __LINE__, __VA_ARGS__);
#else
#define debug_print(fmt, ...) (void)0;
#endif

/* CHANGE_MY_NAME varible is expected to be set to:
 *                old_name:new_name 
 */

#define      chopen_max_renames 10
size_t       chopen_watch_names = 0;

const char* chopen_trigger_key_base = "CHANGE_MY_NAME";
const char* chopen_trigger_value[chopen_max_renames]; 
const char* chopen_new_pname[chopen_max_renames];
size_t      chopen_old_pname_size[chopen_max_renames];
char        chopen_new_rname[PATH_MAX]; /* single copy... hopefully there are no
                                            multiple renames from parallel threads */

typedef int (*orig_open_type)(const char *pathname, int flags, ...);
orig_open_type orig_open;
orig_open_type orig_open64;

/* returns 1 if rename pattern was found,
 *         0 otherwise
 */
int init_rename(const char*  trigger_key, 
                const char** trigger_value,
                const char** new_pname,
                size_t*      old_pname_size) 
{
  *trigger_value = getenv(trigger_key);
  if (*trigger_value) {
    for (*new_pname = *trigger_value; **new_pname && **new_pname != ':'; (*new_pname)++) 
    {
      /* */
    }

    *old_pname_size = *new_pname - *trigger_value;
    *new_pname = **new_pname == ':' ? ++(*new_pname) : 0;

    debug_print("in init_rename(), %s=%s\n", trigger_key, *trigger_value);

    return 1;
  }    

  return 0;
}

void _init(void) {
    // debug_print("in _init(), pid=%d\n", getpid());
    orig_open   = (orig_open_type)dlsym(RTLD_NEXT, "open");
    orig_open64 = (orig_open_type)dlsym(RTLD_NEXT, "open64");

    char key[32];
    for (int i = 0; i < chopen_max_renames; ++i) {
      sprintf(key, "%s_%d", chopen_trigger_key_base, i);
      chopen_watch_names += init_rename( key,
                                         chopen_trigger_value+i,
                                         chopen_new_pname+i,
                                         chopen_old_pname_size+i);
    }
}

/* 
 * returns new name if renaming needs to be done
 *         or zero otherwise
 */
const char* find_new_name(const char* pathname, 
                          const char* trigger_value, 
                          size_t old_pname_size, 
                          const char* new_pname) 
{
  if (new_pname) {
    size_t pathname_size = strlen(pathname);
    if (pathname_size >= old_pname_size && 
        0 == strncmp(pathname + pathname_size - old_pname_size,
                     trigger_value, old_pname_size) ) 
    {
      size_t nbytes = pathname_size - old_pname_size;
      strncpy(chopen_new_rname, pathname, nbytes);

      strncpy(chopen_new_rname + nbytes, new_pname, PATH_MAX - nbytes -1);

      debug_print("chopen: tv=[%s] rp=[%s] np=[%s]\n", trigger_value,
                                                       pathname,
                                                       chopen_new_rname);
      return chopen_new_rname;
    }
    return 0;
  } else {
    return 0;
  }
}

int open_priv(orig_open_type real_open, const char *pathname, int flags, mode_t mode)
{
  debug_print("in open_priv: %s\n", pathname);

  if (chopen_watch_names) {
    for (int i = 0; i < chopen_max_renames; ++i) {
      const char* nn = find_new_name(pathname, 
                                     *(chopen_trigger_value+i), 
                                     *(chopen_old_pname_size+i), 
                                     *(chopen_new_pname+i));
      if (nn) {
        pathname = nn;
        break;
      }
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

