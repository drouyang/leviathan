KITTEN_PATH    = ../kitten
LIBHOBBES_PATH = ../libhobbes
XPMEM_PATH     = ../xpmem
PALACIOS_PATH  = ../palacios
PETLIB_PATH    = ../petlib

CC = gcc
AR = ar

execs := lnx_vm_init

CFLAGS :=	-D_GNU_SOURCE            \
		-Wall -Werror		 \
		-I$(XPMEM_PATH)/include  \
		-I$(LIBHOBBES_PATH)	 \
		-I$(PETLIB_PATH)	 \
		-static

libs  :=	$(LIBHOBBES_PATH)/libhobbes.a

init_objs :=	init.o \
		hobbes_ctrl.o \
		app_launch.o


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

lnx_vm_init: $(init_objs)
	$(call build,CC,$(CC) $(CFLAGS) $^ $(libs) -lm -o $@)


clean:
	rm -f $(wildcard  $(execs)) *.o

