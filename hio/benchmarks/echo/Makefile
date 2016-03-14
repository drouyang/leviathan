LEVIATHAN		= $(PWD)/../../../
HIO			= $(LEVIATHAN)/hio/

CFLAGS   		= -g -Wno-nonnull -Wno-unused-parameter -Wall

LDFLAGS  		= -static


CC       		= gcc
CFLAGS          = -I$(HIO)/include -I$(LEVIATHAN)/libhobbes -I$(LEVIATHAN)/petlib \
				  -I $(LEVIATHAN)/kitten/include -static

LDFLAGS         = -L$(HIO)/lib -lhio_client -L$(LEVIATHAN)/libhobbes -lhobbes \
				  -L$(LEVIATHAN)/kitten/user/install/lib -llwk -lm

SOURCES=echo.c hio.c
#SOURCES=epoll_echo.c hio.c
EXECUTABLES=echo echo-lwk

all: $(EXECUTABLES)
    
echo: $(SOURCES) 
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS) 

echo-lwk: $(OBJECTS) 
	$(CC) -DLWK $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS) 

clean:
	rm -f $(EXECUTABLES) *.o
