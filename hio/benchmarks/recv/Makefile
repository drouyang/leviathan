LEVIATHAN		= $(PWD)/../../../
HIO			= $(LEVIATHAN)/hio/

CFLAGS   		= -g -Wno-nonnull -Wno-unused-parameter -Wall

LDFLAGS  		= -static


CC       		= gcc
CFLAGS          = -I$(HIO)/include -I$(LEVIATHAN)/libhobbes -I$(LEVIATHAN)/petlib \
				  -I $(LEVIATHAN)/kitten/include -static

LDFLAGS         = -L$(HIO)/lib -lhio_client -L$(LEVIATHAN)/libhobbes -lhobbes \
				  -L$(LEVIATHAN)/kitten/user/install/lib -llwk -lm

SOURCES=recv.c hio.c
#SOURCES=epoll_recv.c hio.c
EXECUTABLES=recv recv-lwk

all: $(EXECUTABLES)
    
recv: $(SOURCES) 
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS) 

recv-lwk: $(SOURCES) 
	$(CC) -DLWK $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS) 

clean:
	rm -f $(EXECUTABLES) *.o
