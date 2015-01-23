WHITEDB_PATH = ../whitedb-0.7.3
XPMEM_PATH   = ../xpmem

CFLAGS = -I$(WHITEDB_PATH)/Db -I$(XPMEM_PATH) -static  


CC = gcc
AR = ar

libs := 	$(WHITEDB_PATH)/Main/.libs/libwgdb.a \
		$(XPMEM_PATH)/libxpmem.a

execs :=    	master \
		client


build = \
	@if [ -z "$V" ]; then \
		echo '   [$1]     $@'; \
		$2; \
	else \
		echo '$2'; \
		$2; \
	fi



all: $(execs) $(libs)


% : %.c $(libs)
	$(call build,CC,$(CC) $(CFLAGS)  $<  $(libs) /usr/lib64/libm.a -o $@)

%.o : %.c
	$(call build,CC,$(CC) $(CFLAGS) -c $<  -o $@)

%.a : %.o
	$(call build,AR,$(AR) rcs $@ $^)





clean:
	rm -f $(wildcard  $(execs))
