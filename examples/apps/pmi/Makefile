CFLAGS = -Wall 			\
	-D_GNU_SOURCE		\
	-I../../../libhobbes 	\
	-static

CC = gcc
AR = ar

libs  := ../../../libhobbes/libhobbes.a

execs := test_pmi_hello

test_pmi_hello_objs := test_pmi_hello.o 

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

all:  $(execs) $(libs)

test_pmi_hello: $(test_pmi_hello_objs) $(libs)
	$(call build,CC,$(CC) $(CFLAGS) $^ $(libs) -lm -o $@)

clean:
	rm -f $(wildcard  $(execs)) *.o

FORCE:
