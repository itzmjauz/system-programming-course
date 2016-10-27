/* library.h
 *
 * part of the Systems Programming assignment
 * (c) Vrije Universiteit Amsterdam, 2005-2015. BSD License applies
 * author  : wdb -_at-_ few.vu.nl
 * contact : arno@cs.vu.nl
 * */

// this file define encode/decode filter calling conventions
// make sure that your library routines adhere to these functions
//
// ofcourse, when either is useless you do not have to implement it
// for example, there's no reason to create a decoder for a 44.1->22 kHz convertor

/// an example function prototype for the server
//
// there is NO reason to stick to this typedef
typedef int (*server_filterfunc)(char *, int);

/// a prototype of a filter function for the client
//
// there is NO reason to stick to this typedef
typedef char * (*client_filterfunc)(char *, int, int*);

