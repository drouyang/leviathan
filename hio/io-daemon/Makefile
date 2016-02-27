LEVIATHAN		= $(PWD)/../..

CFLAGS   		= -g -W -Wall -Werror \
				-Wno-nonnull -Wno-unused-parameter

LDFLAGS  		= -static


CC       		= gcc
CFLAGS          = -I../include -I$(LEVIATHAN)/libhobbes -I$(LEVIATHAN)/petlib \
				  -I $(LEVIATHAN)/kitten/include -static

LDFLAGS         = -L../lib -lhio_client -L$(LEVIATHAN)/libhobbes -lhobbes \
				  -L$(LEVIATHAN)/kitten/user/install/lib -llwk -lm

SOURCES			= io-daemon.c
TARGET			= io-daemon


% : %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

all: $(TARGET)

$(TARGET): $(SOURCES)

clean:
	rm -f $(TARGET) *.o
