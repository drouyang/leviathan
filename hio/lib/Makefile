HOBBESDIR 		= $(PWD)/../..
LIBHOBBES 		= $(HOBBESDIR)/libhobbes
PETLIB			= $(HOBBESDIR)/petlib


CFLAGS   		= -g -W -Wall -Werror \
				-Wno-nonnull -Wno-unused-parameter \
				-I$(LIBHOBBES) -I$(PETLIB) -I../include \
				-fPIC
LDFLAGS  		= -lm


CC       		= gcc
AR				= ar

LIBS		    = $(LIBHOBBES)/libhobbes.a \
				$(PETLIB)/petlib.a

STUB			= libhio_stub.a
STUB_OBJS       = libhio_stub.o

CLIENT			= libhio_client.a
CLIENT_OBJS		= libhio_client.o	

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


all: $(STUB) $(CLIENT)

$(STUB): $(STUB_OBJS) $(LIBS)
	$(call build,AR,$(AR) rcs $@ $^)

$(CLIENT): $(CLIENT_OBJS) $(LIBS)
	$(call build,AR,$(AR) rcs $@ $^)

clean:
	rm -f $(STUB) $(CLIENT) *.o
