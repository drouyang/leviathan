CFLAGS   		= -g -W -Wall -Werror \
				-Wno-nonnull -Wno-unused-parameter

LDFLAGS  		= -static


CC       		= gcc
CFLAGS          = -I../include -I../../libhobbes -I../../petlib -static
LDFLAGS         = -L../lib -lhio_client -L../../libhobbes -lhobbes -lm

SOURCES			= io-daemon.c
TARGET			= io-daemon


% : %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

all: $(TARGET)

$(TARGET): $(SOURCES)

clean:
	rm -f $(TARGET) *.o
