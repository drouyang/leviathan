XPMEM_PATH=../xpmem
PALACIOS_PATH=../palacios
PISCES_PATH=../pisces
PETLIB_PATH=../petlib
WHITEDB_PATH=../whitedb-0.7.3


CFLAGS = -Wall -Werror -D_GNU_SOURCE -I$(WHITEDB_PATH)/Db -I$(XPMEM_PATH)/include -I$(PISCES_PATH)/linux_usr -I$(PETLIB_PATH) -I$(PALACIOS_PATH)/user -I../libhobbes -static  


CC = gcc
AR = ar

libs  := 	../libhobbes/libhobbes.a \
		$(PISCES_PATH)/linux_usr/libpisces.a \
		$(PALACIOS_PATH)/user/libv3vee_user.a



execs := 	hobbes


hobbes_objs := 	hobbes.o		\
		app_launch.o		\
		enclave.o		\
		linux_vm_ctrl.o		\
		pisces_vm_ctrl.o        \
		pisces_enclave_ctrl.o


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


hobbes: $(hobbes_objs)
	$(call build,CC,$(CC) $(CFLAGS) $^ $(libs) -lm -o $@)







clean:
	rm -f $(wildcard  $(execs)) *.o

FORCE:
