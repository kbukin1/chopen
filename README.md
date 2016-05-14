# chopen

## A library for changing names of accessed files.

Sometimes there is a need for changing file names an application accesses
without altering application itself. This library does just that. The
library is expected to be loaded by setting `LD_PRELOAD` variable
to `/path/to/the/library/$LIB/chopen.so`. $LIB should not be expanded by
the shell, so enclose it into single quotes. $LIB is replaced by the
loader with `lib` or `lib64` depending on the bitness of the application.
32- and 64-bit versions of the library should be found in lib and
lib64 directories.

File renaming is controlled by `CHANGE_MY_NAME_?` environment
variables (`CHANGE_MY_NAME_0`, `CHANGE_MY_NAME_1` through `CHANGE_MY_NAME_9`). 
`CHANGE_MY_NAME_?` variables should contain pairs of old and new names in the form
of `old_name:new_name`. `old_name` part of the file name is replaced
with `new_name`. `old_name` can be any ending part of a path name used in `open()`,
including full name passed to `open()`.


