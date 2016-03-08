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
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=sockbench sockbench-lwk

all: $(SOURCES) $(EXECUTABLE)
    
sockbench: $(OBJECTS) 
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS) 

sockbench-lwk: $(OBJECTS) 
	$(CC) -DLWK $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS) 

clean:
	rm -f $(TARGET) *.o
