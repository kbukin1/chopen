CC = gcc
LD = ld

CONFIG32        := lib
CONFIG64        := lib64

SOURCES		:= chopenf.c
OBJECTS		:= $(SOURCES:%.c=%.o)

INCLUDES  :=
CFLAGS32	:= $(CFLAGS) -O3 -std=c99 -DPIC -fPIC -m32
CFLAGS64	:= $(CFLAGS) -O3 -std=c99 -DPIC -fPIC

LDFLAGS32	:= -L/usr/lib -shared -ldl -m elf_i386
LDFLAGS64	:= -shared -ldl

CREATE_DIRS 	:= $(shell mkdir -p $(CONFIG32) $(CONFIG64))


all: chopenf32 chopenf64

clean:
	rm -rf $(CONFIG32) $(CONFIG64)

# --------------
chopenf32: $(CONFIG32)/chopenf.so

$(CONFIG32)/chopenf.so: $(SOURCES:%.c=$(CONFIG32)/%.o)
	$(LD) $(LDFLAGS32) -o $@ $^

$(CONFIG32)/%.o:  %.c
	$(CC) $(CFLAGS32) $(INCLUDES) -c -o $@ $<

# --------------
chopenf64: $(CONFIG64)/chopenf.so

$(CONFIG64)/chopenf.so: $(SOURCES:%.c=$(CONFIG64)/%.o)
	$(LD) $(LDFLAGS64) -o $@ $^

$(CONFIG64)/%.o:  %.c
	$(CC) $(CFLAGS64) $(INCLUDES) -c -o $@ $<

