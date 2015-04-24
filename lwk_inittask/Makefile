KITTEN_PATH    = ../kitten
LIBHOBBES_PATH = ../libhobbes
XPMEM_PATH     = ../xpmem
PALACIOS_PATH  = ../palacios
PETLIB_PATH    = ../petlib

CC = gcc
AR = ar

execs := lwk_init

CFLAGS :=	-D_GNU_SOURCE            \
		-Wall -Werror		 \
	 	-I$(KITTEN_PATH)/include \
	  	-I$(PALACIOS_PATH)/user  \
		-I$(XPMEM_PATH)/include  \
		-I$(LIBHOBBES_PATH)	 \
		-I$(PETLIB_PATH)	 \
		-static

libs  :=	$(KITTEN_PATH)/user/install/lib/liblwk.a \
		$(LIBHOBBES_PATH)/libhobbes.a

init_objs :=	init.o \
		pisces.o \
		pisces_ctrl.o \
		hobbes_ctrl.o \
		palacios.o \
		app_launch.o \
		sched_getcpu.o



build = \
        @if [ -z "$V" ]; then \
                echo '   [$1]     $@'; \
                $2; \
        else \
                echo '$2'; \
                $2; \
        fi



% : %.c $(libs)
	$(call build,CC,$(CC) $(CFLAGS)  $<  $(libs) /usr/lib64/libm.a -o $@)

%.o : %.c
	$(call build,CC,$(CC) $(CFLAGS) -c $<  -o $@)

%.a : %.o
	$(call build,AR,$(AR) rcs $@ $^)


all: $(execs) $(libs)

lwk_init: $(init_objs)
	$(call build,CC,$(CC) $(CFLAGS) $^ $(libs) /usr/lib64/libm.a -o $@)


clean:
	rm -f $(wildcard  $(execs)) *.o

