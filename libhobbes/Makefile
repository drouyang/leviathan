XPMEM_PATH?=../xpmem
PALACIOS_PATH?=../palacios
PISCES_PATH?=../pisces
PETLIB_PATH?=../petlib
WHITEDB_PATH?=../whitedb-0.7.3


CFLAGS = -Wall -Werror -Wno-nonnull  \
	-D_GNU_SOURCE                \
	-I$(WHITEDB_PATH)/Db         \
	-I$(XPMEM_PATH)/include      \
	-I$(PISCES_PATH)/linux_usr   \
	-I$(PETLIB_PATH)             \
	-I$(PALACIOS_PATH)/user      \
	-static  


CC = gcc
AR = ar

libs := 	$(XPMEM_PATH)/lib/libxpmem.a         \
		$(WHITEDB_PATH)/Main/.libs/libwgdb.a \
		$(PETLIB_PATH)/petlib.a



target :=    libhobbes.a


libhobbes_objs := 	xemem.o            \
			hobbes.o           \
			hobbes_db.o        \
			hobbes_sys_db.o    \
			hobbes_cmd_queue.o \
			hobbes_enclave.o   \
			hobbes_app.o       \
			hobbes_file.o      \
			hobbes_system.o    \
			hobbes_util.o      \
			pmi.o


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


all: libhobbes.mri $(target) $(libs)

libhobbes.mri: FORCE
	./build_mri.pl libhobbes.mri libhobbes.a $(libhobbes_objs) LIBS $(libs)

libhobbes.a: $(libhobbes_objs) $(libs) libhobbes.mri
	$(call build,AR,$(AR) -M < libhobbes.mri)






clean:
	rm -f $(wildcard  $(target)) *.o

FORCE:
