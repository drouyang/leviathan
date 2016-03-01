LEVIATHAN		= $(PWD)/../../../
HIO			= $(LEVIATHAN)/hio/

CFLAGS   		= -g -W -Wall -Werror \
				-Wno-nonnull -Wno-unused-parameter

LDFLAGS  		= -static


CC       		= gcc
CFLAGS          = -I$(HIO)/include -I$(LEVIATHAN)/libhobbes -I$(LEVIATHAN)/petlib \
				  -I $(LEVIATHAN)/kitten/include -static

LDFLAGS         = -L$(HIO)/lib -lhio_client -L$(LEVIATHAN)/libhobbes -lhobbes \
				  -L$(LEVIATHAN)/kitten/user/install/lib -llwk -lm

SOURCES			= ttcp.c
TARGET			= ttcp


% : %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

all: $(TARGET)

$(TARGET): $(SOURCES)

clean:
	rm -f $(TARGET) *.o
