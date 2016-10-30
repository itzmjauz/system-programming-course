#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include "audio.h"

#define BUFSIZE 1024
#define PORT 32581
#define DONESIZE 4
#define MAX_TIMEOUT 4

static int breakloop = 0; 
const char * DENY = "DENY";
const char * CONNECT = "CONNECT";
const char * FERROR = "FERROR";
const char * VOL = "VOL";
const char * NOISE = "NOISE";
const char * SPEED = "SPEED";

int recvtimeout(int fd) {
  struct timeval timeout;
  fd_set read_set;
  int nb;

  FD_ZERO(&read_set);
  FD_SET(fd, &read_set);
  timeout.tv_sec = MAX_TIMEOUT;
  timeout.tv_usec = 0;

  nb = select(fd + 1, &read_set, NULL, NULL, &timeout);
  if (FD_ISSET(fd, &read_set)) {
    return 1;
  }
  else {
    return nb;
  }
}

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
  int bytesread,  err;
  char buf[BUFSIZE], okmssg[] = "OK";
  socklen_t flen = sizeof(struct sockaddr_in);

  bytesread = recvfrom(server_fd, buf, BUFSIZE, 0, (struct sockaddr *) &addr, &flen);
  while (bytesread > 0) {
    write(audio_fd, buf, bytesread);
    err = sendto(server_fd, okmssg, strlen(okmssg), 0, (struct sockaddr *) &addr, flen);
    if (err < 0) {
      printf("Cannot send OK to server\n");
      return -1;
    }
    err = recvtimeout(server_fd);
    if (err < 0) {
      printf("ERROR : Connection error\n");
      return -1;
    } else if (err == 0) {
      printf("ERROR : Connection lost\n");
      return -1;
    } 
    bytesread = recvfrom(server_fd, buf, BUFSIZE, 0, (struct sockaddr *) &addr, &flen);
    
    if(strncmp(buf, "DONE", DONESIZE) == 0) {
      bytesread = -1;
    }
  }
  printf("Got final packet!\n");

  return 1;
}

int initconnection(int server_fd, char * hostname, char * filename, int filter, char * filterarg) {
  struct sockaddr_in sock;
  int err = 0, audio_fd, sample_rate, sample_size, channels;
  struct in_addr addr;
  char buf[BUFSIZE] = "";
  socklen_t flen = sizeof(struct sockaddr_in);
  addr = resolvehost(hostname);

  sock.sin_family         = AF_INET;
  sock.sin_port           = htons(PORT);
  sock.sin_addr           = addr;

  if((sizeof(filename) + strlen(CONNECT)) < BUFSIZE) {
    strncat(buf, CONNECT, strlen(CONNECT)); //connect message
    strncat(buf, filename, strlen(filename));
  } else {
    printf("Entered filename too long! enter something smaller than %d\n", BUFSIZE);
    return -1;
  }

  err = sendto(server_fd, buf, BUFSIZE, 0, (struct sockaddr *) &sock, flen);
  if (err < 0) {
    printf("Cannot send packets to server!, quitting\n");
    perror("sendto");
    return -1;
  }

  printf("Waiting for answer from server...\n");
  err = recvfrom(server_fd, buf, BUFSIZE, 0, (struct sockaddr *) &sock, &flen);
  if (err < 0) return -1;
  if (strncmp(buf, DENY, strlen(DENY)) == 0) {
    printf("ERROR, Server denied connection, probably busy\n");
    return -1;
  }
  if (strncmp(buf + strlen(CONNECT), filename, strlen(filename)) == 0) {
    printf("Connection OK!\n");
    err = recvfrom(server_fd, buf, BUFSIZE, 0, (struct sockaddr *) &sock, &flen);
    if (err < 0) {
      printf("ERROR : cannot receive audio info from server, exiting..\n");
      return -1;
    }
    if (strncmp(buf, FERROR, strlen(FERROR)) == 0) {
      printf("ERROR : Audiofile not found! Terminating!\n");
      return -1;
    }

    sample_size = strtol(strtok(buf, "|"), NULL, 10);
    sample_rate = strtol(strtok(NULL, "|"), NULL, 10);
    channels = strtol(strtok(NULL, "|"), NULL, 10);
    
    if (filter != 3) {
      audio_fd = aud_writeinit(sample_rate, sample_size, channels);//open audio output
    } else {
      audio_fd = aud_writeinit(sample_rate * strtol(filterarg, NULL, 10) / 100, sample_size, channels);//open audio output
      printf("%d\n", sample_rate * 45 / 100);
    }
    if (audio_fd < 0) {
      printf("ERROR : Cannot open audio output\n");
      return -1;
    }
    
    memset(buf, 0, BUFSIZE);    
    if(filter != 0 && (strlen(filterarg) + 1) < BUFSIZE) {
      buf[0] = (char) filter + '0';
      strncat(buf, filterarg, strlen(filterarg));
      strcat(buf, "\0");
    } else {
      buf[0] = '0';
    }

    err = sendto(server_fd, buf, strlen(buf), 0, (struct sockaddr *) &sock, flen);
    if (err < 0) {
      printf("ERROR : Could not send filter data, terminating\n");
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
  int server_fd, filter = 0;

  printf("Audio client, by Antoni stevenet @ 2016\n"); //logging so that user knows stuff is happening

  signal(SIGINT, sigint_handler);  //catch ctrl-c

  if (argc < 3 || argc > 5) { //check parameters
    printf("ERROR : Incorrect amount of parameters passed\n"); 
    printf("Usage : %s <hostname/IP> <filename> [<filter> [filter options]]\n", argv[0]);
    return -1;
  }

  server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); //server connection
  if (server_fd < 0) {
    printf("ERROR : cannot open input connection");
    return -1;
  }

  if(argv[3]) {
    if(strncmp(argv[3], VOL, strlen(VOL) + 1) == 0) { 
      filter = 1;
      if(!argv[4]) {
        printf("ERROR : Not enough arguments for VOL filter, add percentage of volume to play as argument\n");
        return -1;
      }
      if(strlen(argv[4]) > 3) {
        printf("ERROR : filter argument too big! should be smaller than 1000\n");
        return -1;
      }
    } else if(strncmp(argv[3], NOISE, strlen(NOISE) + 1) == 0) { 
      filter = 2;
      if(!argv[4]) {
        printf("ERROR : Not enough arguments for NOISE filter, add number of HZ to add as sine wave as argument\n");
        return -1;
      }
      if(strlen(argv[4]) > 5) {
        printf("ERROR filter argument too big! should be smaller than 10000 \n");
        return -1;
      }
    } else if(strncmp(argv[3], SPEED, strlen(SPEED) + 1) == 0) { 
      filter = 3;
      if(!argv[4]) {
        printf("ERROR : Not enough arguments for SPEED filter, add percentage to multiply rate by\n");
        return -1;
      }
      if(strlen(argv[4]) > 3) {
        printf("ERROR filter argument too big! should be smaller than 1000\n");
        return -1;
        if(argv[4][0] == '0') {
          printf("SPEED argument can't be 0 or start with 0, retry..\n");
          return -1;
        }
      }
    } else {
      printf("ERROR filter not found! try VOL NOISE or SPEED filter instead\n");
      return -1;
    }
  }

  initconnection(server_fd, argv[1], argv[2], filter, argv[4]);

  return 0;
}
