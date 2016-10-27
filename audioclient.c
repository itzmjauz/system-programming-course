#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "library.h"
#include "audio.h"

#define BUFSIZE 1024
#define PORT 1634
#define DONESIZE 4

static int breakloop = 0; 

void sigint_handler(int sigint) {
  if (breakloop) {  
    printf("SIGINT occurred, shutting down...\n");
    exit(-1);
  }
  breakloop = 1;
  printf("SIGINT caught, closing client.., force quit with ctrl-c\n");
}

struct in_addr resolvehost(char * hostname) {
  struct hostent *resolv;
  struct in_addr *addr;

  resolv = gethostbyname(hostname);
  if (resolv == NULL) {
    //adress not found
    exit(1);
  }
  addr = (struct in_addr*) resolv->h_addr_list[0];

  return *addr;
}
   
int comms(int server_fd, int audio_fd, struct sockaddr_in addr) { //COMMUNICATIONS
  int bytesread, count = 0;
  char buf[BUFSIZE], okmssg[] = "OK";
  socklen_t flen = sizeof(struct sockaddr_in);

  bytesread = recvfrom(server_fd, buf, BUFSIZE, 0, (struct sockaddr *) &addr, &flen);
  while (bytesread > 0) {
    write(audio_fd, buf, bytesread);
    sendto(server_fd, okmssg, BUFSIZE, 0, (struct sockaddr *) &addr, flen);
    count++;
    printf("Got packet: %d\n", count);
    bytesread = recvfrom(server_fd, buf, BUFSIZE, 0, (struct sockaddr *) &addr, &flen);
    
    if(strncmp(buf, "DONE", DONESIZE) == 0) {
      bytesread = -1;
    }
  }
  printf("Got final packet!\n");

  return 1;
}

int initconnection(int server_fd, char * hostname, char * filename) {
  struct sockaddr_in sock;
  int err = 0, audio_fd, sample_rate, sample_size, channels;
  struct in_addr addr;
  char buf[BUFSIZE];
  socklen_t flen = sizeof(struct sockaddr_in);
  addr = resolvehost(hostname);

  sock.sin_family         = AF_INET;
  sock.sin_port           = htons(PORT);
  sock.sin_addr           = addr;

  err = sendto(server_fd, filename, BUFSIZE, 0, (struct sockaddr *) &sock, flen);
  if (err < 0) {
    printf("Cannot send packets to server!, quitting\n");
    return -1;
  }


  printf("Waiting for answer from server...\n");
  err = recvfrom(server_fd, buf, BUFSIZE, 0, (struct sockaddr *) &sock, &flen);
  if (err < 0) return -1;

  if (strncmp(buf, filename, strlen(filename)) == 0) {
    printf("Connection OK!\n");
    err = recvfrom(server_fd, buf, BUFSIZE, 0, (struct sockaddr *) &sock, &flen);
    if (err < 0) {
      printf("ERROR : cannot receive audio info from server, exiting..\n");
      return -1;
    }

    sample_size = strtol(strtok(buf, "|"), NULL, 10);
    sample_rate = strtol(strtok(NULL, "|"), NULL, 10);
    channels = strtol(strtok(NULL, "|"), NULL, 10);

    audio_fd = aud_writeinit(sample_rate, sample_size, channels);//open audio output
    if (audio_fd < 0) {
      printf("ERROR : Cannot open audio output\n");
      return -1;
    }
    comms(server_fd, audio_fd, sock);

  } else {
    printf("Got wrong server response, quitting..\n");
    return -1;
  }

  return -1;
}

int main(int argc, char ** argv) {
  int server_fd, audio_fd, err;

  printf("Audio client, by Antoni stevenet @ 2016\n"); //logging so that user knows stuff is happening

  signal(SIGINT, sigint_handler);  //catch ctrl-c

  if (argc < 3) { //check parameters
    printf("ERROR : Incorrect amount of parameters passed\n"); 
    printf("Usage : %s <hostname/IP> <filename>\n", argv[0]);
    return -1;
  }

  server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); //server connection
  if (server_fd < 0) {
    printf("ERROR : cannot open input connection");
    return -1;
  }
 
  initconnection(server_fd, argv[1], argv[2]);
  //TODO LATER , library stuff

  return 0;
}
