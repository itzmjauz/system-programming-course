/* server.c
 *
 * part of the Systems Programming assignment
 * (c) Vrije Universiteit Amsterdam, 2005-2015. BSD License applies
 * author  : wdb -_at-_ few.vu.nl
 * contact : arno@cs.vu.nl
 * */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <signal.h>
#include <netinet/in.h>

#include "library.h"
#include "audio.h"

/// a define used for the copy buffer in stream_data(...)
#define BUFSIZE 1024

static int breakloop = 0;	///< use this variable to stop your wait-loop. Occasionally check its value, !1 signals that the program should close

/// stream data to a client. 
///
/// This is an example function; you do *not* have to use this and can choose a different flow of control
///
/// @param fd an opened file descriptor for reading and writing
/// @return returns 0 on success or a negative errorcode on failure
int stream_data(int client_fd)
{
	int data_fd;
	int channels, sample_size, sample_rate;
	server_filterfunc pfunc;
	char *datafile, *libfile;
	char buffer[BUFSIZE];
	
	// TO IMPLEMENT
	// receive a control packet from the client 
	// containing at the least the name of the file to stream and the library to use
	{
		datafile = strdup("1.wav");
		libfile = NULL;
	}
	
	// open input
	data_fd = aud_readinit(datafile, &sample_rate, &sample_size, &channels);
	if (data_fd < 0){
		printf("failed to open datafile %s, skipping request\n",datafile);
		return -1;
	}
	printf("opened datafile %s\n",datafile);

	// optionally open a library
	if (libfile){
		// try to open the library, if one is requested
		pfunc = NULL;
		if (!pfunc){
			printf("failed to open the requested library. breaking hard\n");
			return -1;
		}
		printf("opened libraryfile %s\n",libfile);
	}
	else{
		pfunc = NULL;
		printf("not using a filter\n");
	}
	
	// TO IMPLEMENT : optionally return an error code to the client if initialization went wrong
	
	// start streaming
	{
		int bytesread, bytesmod;
		
		bytesread = read(data_fd, buffer, BUFSIZE);
		while (bytesread > 0){
			// you might also want to check that the client is still active, whether it wants resends, etc..
			
			// edit data in-place. Not necessarily the best option
			if (pfunc)
				bytesmod = pfunc(buffer,bytesread); 
			write(client_fd, buffer, bytesmod);
			bytesread = read(data_fd, buffer, BUFSIZE);
		}
	}

	// TO IMPLEMENT : optionally close the connection gracefully 	
	
	if (client_fd >= 0)
		close(client_fd);
	if (data_fd >= 0)
		close(data_fd);
	if (datafile)
		free(datafile);
	if (libfile)
		free(libfile);
	
	return 0;
}

/// unimportant: the signal handler. This function gets called when Ctrl^C is pressed
void sigint_handler(int sigint)
{
	if (!breakloop){
		breakloop=1;
		printf("SIGINT caught. Please wait to let the server close gracefully.\nTo close hard press Ctrl^C again.\n");
	}
	else{
       		printf ("SIGINT occurred, exiting hard... please wait\n");
		exit(-1);
	}
}

/// the main loop, continuously waiting for clients
int main (int argc, char **argv)
{
	printf ("SysProg network server\n");
	printf ("handed in by Antoni stevenet\n");
	
	signal(SIGINT, sigint_handler );	// trap Ctrl^C signals
	
	while (!breakloop){
		// TO IMPLEMENT: 
		// 	wait for connections
		// 	when a client connects, start streaming data (see the stream_data(...) prototype above)
		sleep(1);
		
	}

	return 0;
}

