###################### DEFS

CC = gcc
CFLAGS = -Wall -Werror
LDFLAGS = -ldl

####################### TARGETS

.PHONY : all clean distclean

all : client.c server.c
	$(CC) $(CFLAGS) -o client client.c -s audio.c
	$(CC) $(CFLAGS) -o server server.c -s audio.c -lm

distclean : clean
	rm -f server client *.so
clean:
	rm -f $(OBJECTS) server client *.o 

