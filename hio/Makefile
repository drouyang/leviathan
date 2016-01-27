TARGET  = hio

HOBBESDIR = $(PWD)/../libhobbes/
LIBHOBBES = $(HOBBESDIR)/libhobbes.a

OBJ     = $(TARGET).o
SRC     = $(TARGET).c 

CC       = gcc
CFLAGS   = -I$(HOBBESDIR) -fPIC -pie -lm
LDFLAGS  =


$(TARGET): $(OBJ) $(LIBHOBBES)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) 

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) $^ -c -o $@

clean:
	rm -f *.o *.~ $(TARGET)
