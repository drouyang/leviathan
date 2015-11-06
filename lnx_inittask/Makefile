KITTEN_PATH    ?= ../kitten
LIBHOBBES_PATH ?= ../libhobbes
XPMEM_PATH     ?= ../xpmem
PALACIOS_PATH  ?= ../palacios
PETLIB_PATH    ?= ../petlib
PISCES_PATH    ?= ../pisces
WHITEDB_PATH   ?=../whitedb-0.7.3

CC = gcc
AR = ar

execs := lnx_init

CFLAGS :=	-D_GNU_SOURCE              \
		-g -W -Wall -Werror	   \
		-Wno-unused-parameter -Wno-nonnull \
		-I$(XPMEM_PATH)/include    \
		-I$(LIBHOBBES_PATH)	   \
		-I$(PETLIB_PATH)	   \
	        -I$(WHITEDB_PATH)/Db       \
		-I$(PALACIOS_PATH)/user    \
		-I$(PISCES_PATH)/linux_usr \
		-static

libs  :=	$(LIBHOBBES_PATH)/libhobbes.a         \
		$(PISCES_PATH)/linux_usr/libpisces.a  \
                $(PALACIOS_PATH)/user/libv3vee_user.a \
		$(PETLIB_PATH)/petlib.a               

init_objs :=	init.o \
		hobbes_ctrl.o \
		lnx_app.o \
		file_io.o \
		palacios.o \
		master.o \


build = \
        @if [ -z "$V" ]; then \
                echo '   [$1]     $@'; \
                $2; \
        else \
                echo '$2'; \
                $2; \
        fi



% : %.c $(libs)
	$(call build,CC,$(CC) $(CFLAGS)  $<  $(libs) -lm -o $@)

%.o : %.c
	$(call build,CC,$(CC) $(CFLAGS) -c $<  -o $@)

%.a : %.o
	$(call build,AR,$(AR) rcs $@ $^)


all: $(execs) $(libs)

lnx_init: $(init_objs)
	$(call build,CC,$(CC) $(CFLAGS) $^ $(libs) -lm -o $@)


clean:
	rm -f $(wildcard  $(execs)) *.o

