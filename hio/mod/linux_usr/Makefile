
CFLAGS = -static -Wall -Werror -D_GNU_SOURCE -I../include \
         -I$(PETLIB_PATH) -L$(PETLIB_PATH) -I$(LIBHOBBES) -L$(LIBHOBBES)


CC = gcc
AR = ar

execs := 	stub \
                engine \
                engine_client \
                test_xemem \
                test_syscall \


ext_libs := $(PETLIB_PATH)/petlib.a $(LIBHOBBES)/libhobbes.a -lm

build = \
	@if [ -z "$V" ]; then \
		echo '   [$1]     $@'; \
		$2; \
	else \
		echo '$2'; \
		$2; \
	fi

ifeq ($(STATIC),y)
  CFLAGS        += -static
  CURSES_CFLAGS  = -DNCURSES_STATIC
endif



all: $(execs)

% : %.c
	$(call build,CC,$(CC) $(CFLAGS)  $<  $(libs) $(ext_libs) -o $@)

%.o : %.c
	$(call build,CC,$(CC) $(CFLAGS) -c $<  -o $@)

%.a : %.o
	$(call build,AR,$(AR) rcs $@ $^)


clean:
	rm -f $(wildcard  $(execs)) $(wildcard $(libpisces-objs)) *.a
