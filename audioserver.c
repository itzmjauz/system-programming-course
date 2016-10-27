#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/select.h>

#include "library.h"
#include "audio.h"

#define BUFSIZE 1024
#define PORT 1634

static int breakloop = 0;

int recvtimeout(int fd) {
  struct timeval timeout;
  fd_set read_set;
  int nb;

  FD_ZERO(&read_set);
  FD_SET(fd, &read_set);
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  nb = select(fd + 1, &read_set, NULL, NULL, &timeout);
  if (FD_ISSET(fd, &read_set)) {
    return 1;
  }
  else {
    return nb;
  }
}

int streamfile(int fd, int data_fd, struct sockaddr_in addr, float sleeptime) {
  int bytesread, err = 0, count = 0;
  socklen_t flen = sizeof(struct sockaddr_in);
  char buf[BUFSIZE], okmssg[] = "OK";

  bytesread = read(data_fd, buf, BUFSIZE);
  while (bytesread > 0) {
    usleep(sleeptime);
    err = sendto(fd, buf, BUFSIZE, 0, (struct sockaddr *) &addr, flen);
    if (err < 0) {
      printf("ERROR : cannot send audio packet, skipping request\n");
      return -1;
    }
    printf("Sending packet ...");   // not doing this to remove delay in audioplay
    err = recvtimeout(fd);
    if (err < 0) {
      printf("ERROR : Connection error\n");
      return -1;
    } else if (err == 0) {
      printf("Packet : lost\n");
    } else {
      err = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *) &addr, &flen);
      if (err < 0) {
        printf("ERROR : Connection error\n");
      }
      printf(" packet %d correctly received!\n", count);
    }
    if(strncmp(buf, okmssg, 2) != 0) {
      printf("\nNo OK received, skipping request\n");
      return -1;
    }
    count++;

    bytesread = read(data_fd, buf, BUFSIZE);
  }

  printf("Sending DONE packet to client...\n");
  strncpy(buf, "DONE", 4);
  err = sendto(fd, buf, BUFSIZE, 0, (struct sockaddr *) &addr, flen); 
  
  if (err < 0) {
    printf("ERROR : failed sending DONE packet, continue anyway..\n");
    return -1;
  } else {
    printf("Succcesfuly streamed audiofile, waiting for new connections..\n");
    return -1;
  }
}

int stream_audio(int fd, socklen_t flen, char * filename, struct sockaddr_in addr) {
  int data_fd, channels, sample_size, sample_rate, bitrate, err = 0;
  float sleeptime;
  char buf[BUFSIZE];

  data_fd = aud_readinit(filename, &sample_rate, &sample_size, &channels);
  if (data_fd < 0) {
    printf("ERROR : failed to open audio file : %s\n, Skipping request\n", filename);
    return -1;
  }

  printf("Audiofile found! sending details...\n");
  sprintf(buf, "%d|%d|%d|", sample_size, sample_rate, channels);

  err = sendto(fd, buf, BUFSIZE, 0, (struct sockaddr *) &addr, flen);
  
  bitrate = sample_size * sample_rate * channels;
  sleeptime = (1000000 * (float) (BUFSIZE * 8)) / (float) bitrate; //time per packet in microseconds

  streamfile(fd, data_fd, addr, sleeptime);
}

void connect_client(int fd, socklen_t flen) {
  int err = 0;
  char buf[BUFSIZE];
  struct sockaddr_in addr;
  
  err = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *) &addr, &flen);
  if (err < 0) {
    printf("ERROR : %d, when attempting initial connection\n", err);
    connect_client(fd, flen);
  } else {
    printf("Connected to host : %s, port : %d: %s\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), buf);
    err = sendto(fd, buf, BUFSIZE, 0, (struct sockaddr *) &addr, flen);
    
    if (err < 0) {
      printf("Cannot send messages to host, skipping client\n");
      connect_client(fd, flen);
      return;
    }

    stream_audio(fd, flen, buf, addr);
    connect_client(fd, flen);
    return;
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

int main(int argc, char ** argv) {
  struct sockaddr_in addr;
  int fd, err = 0;
  socklen_t flen;

  printf("Audio server, by Antoni Stevenet @ 2016\n");

  signal(SIGINT, sigint_handler); //catch ctrl-c

  fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); //server connection
  if (fd < 0) {
    printf("ERROR : cannot open input connection\n");
    return -1;
  }

  addr.sin_family         = AF_INET;
  addr.sin_port           = htons(PORT);
  addr.sin_addr.s_addr    = htonl(INADDR_ANY);

  printf("Binding socket..\n");
  flen = sizeof(struct sockaddr_in);
  err = bind(fd, (struct sockaddr *) &addr, flen);
  if (err < 0) {
    printf("ERROR : cannot bind socket\n");
    close(fd);
    return -1;
  }

  printf("Starting communication loop\n");
  while (!breakloop) {
    //wait for connection
    //after connection stream 
    connect_client(fd, flen);
    sleep(1);
  }

  return 0;
}

