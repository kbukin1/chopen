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
FILE* chopen_debug_output;
#define debug_print(fmt, ...) fprintf(chopen_debug_output, "[%d]%s:%d: " fmt, getpid(),__FILE__, __LINE__, __VA_ARGS__);
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
char        chopen_trigger_value_loc[2*PATH_MAX]; /* almost enough for two maximum paths */

typedef int (*orig_open_type)(const char *pathname, int flags, ...);
orig_open_type orig_open;
orig_open_type orig_open64;

typedef int (*orig_rename_type)(const char *old_path, const char *new_path);
orig_rename_type orig_rename;

#ifdef DEBUG
typedef int (*orig_unlink_type)(const char*);
orig_unlink_type orig_unlink;

typedef int (*orig_creat_type)(const char *pathname, mode_t mode);
orig_creat_type orig_creat;
#endif

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
    strncpy(chopen_trigger_value_loc, *trigger_value, 2*PATH_MAX); /* cannot trust this string will */
    *trigger_value = chopen_trigger_value_loc;                     /* be kept for us to use later */
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
#ifdef DEBUG
    chopen_debug_output = fopen("/tmp/chopen.txt", "a");
#endif
    // debug_print("in _init(), pid=%d\n", getpid());
    orig_open   = (orig_open_type)dlsym(RTLD_NEXT, "open");
    orig_open64 = (orig_open_type)dlsym(RTLD_NEXT, "open64");
    orig_rename  = (orig_rename_type)dlsym(RTLD_NEXT,"rename");
#ifdef DEBUG
    orig_unlink = (orig_unlink_type)dlsym(RTLD_NEXT,"unlink");
    orig_creat  = (orig_creat_type)dlsym(RTLD_NEXT,"creat");
#endif

    char key[32];
    for (int i = 0; i < chopen_max_renames; ++i) {
      sprintf(key, "%s_%d", chopen_trigger_key_base, i);
      chopen_watch_names += init_rename( key,
                                         chopen_trigger_value+i,
                                         chopen_new_pname+i,
                                         chopen_old_pname_size+i);
    }
}

void _fini(void) {
#ifdef DEBUG
    fclose(chopen_debug_output);
#endif
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
  debug_print("in open_priv: %s [%d]\n", pathname, chopen_watch_names);

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
  // debug_print("in open [%s]\n", pathname);
  return open_priv(orig_open, pathname, flags, mode);
}

int open64(const char * pathname, int flags, mode_t mode)
{
  // debug_print("in open64 [%s]\n", pathname);
  return open_priv(orig_open64, pathname, flags, mode);
}

int rename(const char* old_path, const char* new_path)
{
  debug_print("in rename [%s] [%s]\n", old_path, new_path);

  const char* orig_new_path = 0;
  if (chopen_watch_names) {
    for (int i = 0; i < chopen_max_renames; ++i) {
      const char* nn = find_new_name(old_path, 
                                     *(chopen_trigger_value+i), 
                                     *(chopen_old_pname_size+i), 
                                     *(chopen_new_pname+i));
      if (nn) {
        old_path = nn;
        break;
      }
    }

    for (int i = 0; i < chopen_max_renames; ++i) {
      const char* nn = find_new_name(new_path, 
                                     *(chopen_trigger_value+i), 
                                     *(chopen_old_pname_size+i), 
                                     *(chopen_new_pname+i));
      if (nn) {
        orig_new_path = new_path;
        new_path = nn;
        break;
      }
    }

  }

  int ret = orig_rename(old_path, new_path);
  if (orig_new_path)
    unlink(orig_new_path);

  return ret;
}

#ifdef DEBUG
int creat(const char * pathname, mode_t mode)
{
  debug_print("in creat [%s]\n", pathname);
  return orig_creat(pathname, mode);
}

#if 0
int unlink(const char * pathname)
{
  debug_print("in unlink [%s]\n", pathname);
  return orig_unlink(pathname);
}
#endif
#endif

