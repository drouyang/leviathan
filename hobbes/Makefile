WHITEDB_PATH = ../whitedb-0.7.3
XPMEM_PATH   = ../xpmem
PISCES_PATH  = $(HOME)/pisces/pisces/linux_usr
PETLIB_PATH  = $(HOME)/pisces/petlib

CFLAGS = -I$(WHITEDB_PATH)/Db -I$(XPMEM_PATH) -I$(PISCES_PATH) -I$(PETLIB_PATH) -static  


CC = gcc
AR = ar

libs := 	$(WHITEDB_PATH)/Main/.libs/libwgdb.a \
		$(XPMEM_PATH)/libxpmem.a \
		$(PISCES_PATH)/libpisces.a \
		$(PETLIB_PATH)/petlib.a



execs :=    	master \
		libclient.a \
		hobbes \


hobbes_objs := 		hobbes.o  \
			enclave.o \
			hobbes_db.o \
			client.o


master_objs := 		master.o    \
			hobbes_db.o \


libclient_objs := 	client.o \
			hobbes_db.o

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

master: $(master_objs)
	$(call build,CC,$(CC) $(CFLAGS) $^ $(libs) /usr/lib64/libm.a -o $@)

hobbes: $(hobbes_objs)
	$(call build,CC,$(CC) $(CFLAGS) $^ $(libs) /usr/lib64/libm.a -o $@)

libclient.a: $(libclient_objs)
	$(call build,AR,$(AR) rcs $@ $^)







clean:
	rm -f $(wildcard  $(execs)) *.o
