CFLAGS   		= -g -W -Wall -Werror \
				-Wno-nonnull -Wno-unused-parameter

LDFLAGS  		= -static


CC       		= gcc

SOURCES			= app.c
TARGET			= app


% : %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

all: $(TARGET)

$(TARGET): $(SOURCES)

clean:
	rm -f $(TARGET) *.o
