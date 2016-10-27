###################### DEFS

CC = gcc
CFLAGS = -Wall -Werror
LDFLAGS = -ldl

####################### TARGETS

.PHONY : all clean distclean

all : audioclient.c audioserver.c
	$(CC) $(CFLAGS) -o client audioclient.c -s audio.c
	$(CC) $(CFLAGS) -o server audioserver.c -s audio.c

distclean : clean
	rm -f server client *.so
clean:
	rm -f $(OBJECTS) server client *.o 

