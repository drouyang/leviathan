XPMEM_PATH?=../xpmem
PALACIOS_PATH?=../palacios
PISCES_PATH?=../pisces
PETLIB_PATH?=../petlib
WHITEDB_PATH?=../whitedb-0.7.3


CFLAGS = 	-g			\
		-D_GNU_SOURCE		\
	 	-Wall -Werror          	\
		-I$(WHITEDB_PATH)/Db   	\
		-I$(PETLIB_PATH) 	\
		-I../libhobbes		\
		-static  


CC = gcc
AR = ar

libs  := ../petlib/petlib.a



execs := 	hobbes-gui


hobbes_objs := 	main.o


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


hobbes-gui: $(hobbes_objs)
	$(call build,CC,$(CC) $(CFLAGS) $^ $(libs) -lm -o $@)







clean:
	rm -f $(wildcard  $(execs)) *.o

FORCE:
