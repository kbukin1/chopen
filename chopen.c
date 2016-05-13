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

#define DEBUG 1

#ifdef DEBUG
#define debug_print(fmt, ...) printf("%s:%d: " fmt, __FILE__, __LINE__, __VA_ARGS__);
#else
#define debug_print(fmt, ...) (void)0;
#endif

#define rename_failed(filename, fmt, ...) printf("Error (%s:%d): " \
                                          fmt, __FILE__, __LINE__, __VA_ARGS__); \
                                          return orig_unlink(filename);
typedef int (*orig_open_type)(const char *pathname, int flags);

orig_open_type orig_open;

void _init(void) {
    // debug_print("in _init(), pid=%d\n", getpid());
    orig_open = (orig_open_type)dlsym(RTLD_NEXT, "open");
}

int open(const char *pathname, int flags, ...)
{
  debug_print("in open: %s\n", pathname);
  return orig_open(pathname, flags);
}

