HOBBESDIR		= $(PWD)/../../../
LIBHOBBESDIR	= $(HOBBESDIR)/libhobbes
PETLIBDIR		= $(HOBBESDIR)/petlib
LIBHOBBES		= $(LIBHOBBESDIR)/libhobbes.a
PETLIB			= $(PETLIBDIR)/petlib.a

CFLAGS   		= -g -W -Wall -Werror \
				-Wno-nonnull -Wno-unused-parameter \
				-I../include -I$(LIBHOBBESDIR) -I$(PETLIBDIR) -I$(HOBBESDIR)/hio/include\
				-fPIC -pie
LDFLAGS  		= -lm

CC       		= gcc
TARGET			= stub

LIBS		   := $(HOBBESDIR)/hio/lib/libhio_stub.a $(LIBHOBBES) $(PETLIB)

TARGET_OBJS    := stub.o	

build = \
	@if [ -z "$V" ]; then \
		echo '   [$1]     $@'; \
		$2; \
	else \
		echo '$2'; \
		$2; \
	fi



% : %.c $(LIBS)
	$(call build,CC,$(CC) $(CFLAGS) $< $(LIBS) -o $@ $(LDFLAGS))

%.o: %.c
	$(call build,CC,$(CC) $(CFLAGS) -c $< -o $@)

all: $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET) *.o
