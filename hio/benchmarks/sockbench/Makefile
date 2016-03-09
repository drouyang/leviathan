LEVIATHAN		= $(PWD)/../../../
HIO			= $(LEVIATHAN)/hio/

CFLAGS   		= -g -Wno-nonnull -Wno-unused-parameter -Wall

LDFLAGS  		= -static


CC       		= gcc
CFLAGS          = -I$(HIO)/include -I$(LEVIATHAN)/libhobbes -I$(LEVIATHAN)/petlib \
				  -I $(LEVIATHAN)/kitten/include -static

LDFLAGS         = -L$(HIO)/lib -lhio_client -L$(LEVIATHAN)/libhobbes -lhobbes \
				  -L$(LEVIATHAN)/kitten/user/install/lib -llwk -lm

SOURCES=server.c hio.c
EXECUTABLES=sockbench sockbench-lwk

all: $(EXECUTABLES)
    
sockbench: $(SOURCES) 
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS) 

sockbench-lwk: $(OBJECTS) 
	$(CC) -DLWK $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS) 

clean:
	rm -f $(EXECUTABLES) *.o
