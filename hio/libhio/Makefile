HOBBESDIR 		= $(PWD)/../..
LIBHOBBES 		= $(HOBBESDIR)/libhobbes
PETLIB			= $(HOBBESDIR)/petlib


CFLAGS   		= -g -W -Wall -Werror \
				-Wno-nonnull -Wno-unused-parameter \
				-I$(LIBHOBBES) -I$(PETLIB) -fPIC
LDFLAGS  		= -lm


CC       		= gcc
AR				= ar
TARGET			= libhio.a

LIBS		   := $(LIBHOBBES)/libhobbes.a \
				$(PETLIB)/petlib.a

LIBHIO_OBJS    := libhio.o	

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

%.a: %.o
	$(call build,AR,$(AR) rcs $@ $^)


all: $(TARGET) $(LIBHOBBES)

$(TARGET): $(LIBHIO_OBJS) $(LIBS)
	$(call build,AR,$(AR) rcs $@ $^)

clean:
	rm -f $(TARGET) *.o
