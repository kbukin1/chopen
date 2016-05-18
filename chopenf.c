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
FILE* chopen_debug_output;
#define debug_print(fmt, ...) printf("[%d]%s:%d: " fmt, getpid(),__FILE__, __LINE__, __VA_ARGS__);
//#define debug_print(fmt, ...) fprintf(chopen_debug_output, "[%d]%s:%d: " fmt, getpid(),__FILE__, __LINE__, __VA_ARGS__);
#else
#define debug_print(fmt, ...) (void)0;
#endif

/* 
 * CHANGE_MY_NAME varible containing file name with old_name:new_name entries
 */

#define     chopen_max_renames      1024
#define     chopen_temp_buff_size   2*PATH_MAX
size_t      chopen_watch_names   = 0;
const char* chopen_trigger_name  = "CHANGE_MY_NAME";
struct chname {
  size_t      old_size;
  const char* old_name;
  const char* new_name;
};

char chopen_buffer1[PATH_MAX];
char chopen_buffer2[PATH_MAX];

struct chname chnames[chopen_max_renames];

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


size_t init_rename_table(const char* trigger_key) {
  size_t ret = 0;
  const char *filename = getenv(trigger_key);

  if (!filename) 
    return ret;

  debug_print("arg file: %s\n", filename);
  FILE* fp = fopen(filename, "r");
  if (fp == 0)
    return ret;

  char temp_buffer[chopen_temp_buff_size];

  while (fgets(temp_buffer, chopen_temp_buff_size, fp)) {
    temp_buffer[strcspn(temp_buffer, "\n")] = 0;
    char* delim = strchr(temp_buffer, ':');
    if (delim) {
      size_t buff_size = strlen(temp_buffer) + 1;
      char* buff = malloc(buff_size);
      strcpy(buff, temp_buffer);
      delim = strchr(buff, ':');
      delim++;

      struct chname* chn = (chnames + ret++);
      chn->old_size = delim - buff -1;
      chn->old_name = buff;
      chn->new_name = delim;

      if (ret >= chopen_max_renames)
        exit(1);

      debug_print("in init_rename_table: os[%zu] on[%s] nn[%s] ret[%zu]\n", 
          chn->old_size, chn->old_name, chn->new_name, ret);
    }
  }

  fclose(fp);

  return ret;
}

void _init(void) {
#ifdef DEBUG
  chopen_debug_output = fopen("/tmp/chopen.txt", "a");
  orig_unlink = (orig_unlink_type)dlsym(RTLD_NEXT,"unlink");
  orig_creat  = (orig_creat_type)dlsym(RTLD_NEXT,"creat");
#endif
  // debug_print("in _init(), pid=%d\n", getpid());
  orig_open   = (orig_open_type)dlsym(RTLD_NEXT, "open");
  orig_open64 = (orig_open_type)dlsym(RTLD_NEXT, "open64");
  orig_rename = (orig_rename_type)dlsym(RTLD_NEXT,"rename");

  chopen_watch_names = init_rename_table(chopen_trigger_name);
}

void _fini(void) {
#ifdef DEBUG
  fclose(chopen_debug_output);

  for (int i = 0; i < chopen_watch_names; ++i) {
    free( (void*) (chnames + i)->old_name );
  }
#endif
}

const char* find_new_name(const char* pathname, char* temp_buffer) {

  for (int i = 0; i < chopen_watch_names; ++i) {
    struct chname* ch = chnames + i;
    size_t pathname_size = strlen(pathname);
    if (pathname_size >= ch->old_size &&
       0 == strncmp(pathname + pathname_size - ch->old_size, ch->old_name, ch->old_size))
    {
      size_t nbytes = pathname_size - ch->old_size;
      strncpy(temp_buffer, pathname, nbytes);

      strncpy(temp_buffer + nbytes, ch->new_name, PATH_MAX - nbytes -1);

      debug_print("rename found: nn=[%s] on=[%s] con=[%s]\n",
          temp_buffer, pathname, ch->old_name);

      return temp_buffer;
    }
  }

  return 0;
}

int open_priv(orig_open_type real_open, const char *pathname, int flags, mode_t mode)
{
  debug_print("in open_priv: %s [%d]\n", pathname, chopen_watch_names);

  const char* temp = find_new_name(pathname, chopen_buffer1);
  pathname = temp ? temp : pathname;

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

  const char* temp = find_new_name(old_path, chopen_buffer1);
  old_path = temp ? temp : old_path;

  const char* orig_new_path = new_path;
  temp = find_new_name(new_path, chopen_buffer2);
  new_path = temp ? temp : new_path;

  int ret = orig_rename(old_path, new_path);

  if (temp)
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

