/* libblank.c
 *
 * part of the Systems Programming assignment
 * (c) Vrije Universiteit Amsterdam, 2005-2015. BSD License applies
 * author  : wdb -_at-_ few.vu.nl
 * contact : arno@cs.vu.nl
 * */

#include <stdio.h>

#include "library.h"

/* library's initialization function */
void _init()
{
	printf("Initializing library\n");
}

int encode(char* buffer, int bufferlen)
{
	return bufferlen;
}

char *decode(char* buffer, int bufferlen, int *outbufferlen)
{  
	*outbufferlen = bufferlen;
	return buffer;
}


/* library's cleanup function */
void _fini()
{
	printf("Cleaning out library\n");
}

