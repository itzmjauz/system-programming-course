#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>

#include "audio.h"

#define BUFSIZE 1024
#define PORT 32581
#define MAX_PACKET_LOSS 3

static int breakloop = 0;
static int filter = 0;
static int filterarg = 0;
static int sample_size = 0;
static int sample_rate = 0;
char * DONE = "DONE";
char * CONNECT = "CONNECT";
char * FERROR = "FERROR";
char * DENY = "DENY";
char * OK = "OK";

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

float getPercent(int source) {
  float percentage;
  percentage = (float) source / 100; 
  return percentage; 
}

char * noise(char * buffer) {
  if(sample_size == 16) {
    for(int i = 0 ; i < BUFSIZE ; i++) {
      buffer[i] += (int16_t) (SHRT_MAX * 0.1f * sinf(2.0f * M_PI * filterarg * i / sample_rate));
    }
  } else if(sample_size == 8) {
    for(int i = 0 ; i < BUFSIZE ; i++) {
      buffer[i] += (int8_t) (SHRT_MAX * 0.1f * sinf(2.0f * M_PI * filterarg * i / sample_rate));
    }
  } else {
    printf("ERROR : Filter does not support given sample size\n");
    exit(-1);
  }

  return buffer;
}

char * vol(char * buffer) {
  int sampleCount;
  float percent = getPercent(filterarg);
  
  if(sample_size == 16) {
    int16_t * samp = (int16_t *) &buffer[0];
    sampleCount = (BUFSIZE / sizeof(int16_t));
    for (int i = 0; i < sampleCount; i++) {
      samp[i] = (int16_t) (samp[i] * percent);
    }
  } else {
    int8_t * samp = (int8_t *) &buffer[0];
    sampleCount = (BUFSIZE / sizeof(int8_t));
    for (int i = 0; i < sampleCount; i++) {
      samp[i] = (int8_t) (samp[i] * percent);
    }
  }
   
  return buffer;
}

float adjustsleep(float sleeptime) {
  return sleeptime / getPercent(filterarg);
}

char * filterfunc(char * buffer) {
  if(filter == 1) return vol(buffer);
  if(filter == 2) return noise(buffer);
  else return buffer;
}

int streamfile(int fd, int data_fd, struct sockaddr_in addr, float sleeptime) {
  int bytesread, err = 0, lostcount = 0;
  socklen_t flen = sizeof(struct sockaddr_in);
  char buf[BUFSIZE], * filtered;

  if(filter == 3) sleeptime = adjustsleep(sleeptime); 
  bytesread = read(data_fd, buf, BUFSIZE);
  while (bytesread > 0) {
    filtered = filterfunc(buf);
    usleep(sleeptime);
    err = sendto(fd, filtered, BUFSIZE, 0, (struct sockaddr *) &addr, flen);
    if (err < 0) {
      printf("ERROR : cannot send audio packet, skipping request\n");
      return -1;
    }
    err = recvtimeout(fd);
    if (err < 0) {
      printf("ERROR : Connection error\n");
      return -1;
    } else if (err == 0) {
      printf("Packet : lost\n");
      lostcount++;
      if(lostcount > MAX_PACKET_LOSS) {
        printf("Connection lost, skipping request\n");
        return -1;
      }
    } else {
      err = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *) &addr, &flen);
      if (err < 0) {
        printf("ERROR : Connection error\n");
      }
    }
    if(strncmp(buf, OK, strlen(OK)) != 0) {
      if(strncmp(buf, CONNECT, strlen(CONNECT)) == 0) {
        printf("Sending DENY to new client\n");
        err = sendto(fd, DENY, strlen(DENY), 0, (struct sockaddr *) &addr, flen);
        if(err < 0) {
          printf("ERROR, Connection error\n");
        }
      } else {
        if(lostcount > MAX_PACKET_LOSS) {
          printf("Connection lost, skipping request\n");
          return -1;
        }
        printf("No OK received, retrying\n");
        lostcount++;
      }
    } else {
      lostcount = 0;
    }

    if(lostcount == 0) bytesread = read(data_fd, buf, BUFSIZE);
  }

  printf("Sending DONE packet to client...\n");
  err = sendto(fd, DONE, strlen(DONE), 0, (struct sockaddr *) &addr, flen); 
  
  if (err < 0) {
    printf("ERROR : failed sending DONE packet, continue anyway..\n");
    return -1;
  } else {
    printf("Succcesfuly streamed audiofile, waiting for new connections..\n");
    return -1;
  }
}

int stream_audio(int fd, socklen_t flen, char * filename, struct sockaddr_in addr) {
  int data_fd, channels, bitrate, err;
  float sleeptime;
  char buf[BUFSIZE];

  data_fd = aud_readinit(filename + strlen(CONNECT), &sample_rate, &sample_size, &channels);
  if (data_fd < 0) {
    printf("ERROR : failed to open audio file : %s\n, Skipping request\n", filename + strlen(CONNECT));
    err = sendto(fd, FERROR, strlen(FERROR), 0, (struct sockaddr *) &addr, flen);
    if (err < 0) {
      printf("ERROR : Couldn't send File error message\n");
      return -1;
    }
    return -1;
  }

  printf("Audiofile found! sending details...\n");
  sprintf(buf, "%d|%d|%d|", sample_size, sample_rate, channels);

  err = sendto(fd, buf, BUFSIZE, 0, (struct sockaddr *) &addr, flen);
  if (err < 0) {
    printf("Could not send packet...Skipping request\n");
    return -1;
  }
  //read filter stuff
  memset(buf, 0, BUFSIZE);
  err = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *) &addr, &flen);
  filter = buf[0] - '0';
  filterarg = strtol(buf + 1, NULL, 10);
   
  bitrate = sample_size * sample_rate * channels;
  sleeptime = (1000000 * (float) (BUFSIZE * 8)) / (float) bitrate; //time per packet in microseconds
  streamfile(fd, data_fd, addr, sleeptime);
  return 1;
}

void connect_client(int fd, socklen_t flen) {
  int err = 0;
  char buf[BUFSIZE];
  struct sockaddr_in addr;
  
  err = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *) &addr, &flen);
  if (err < 0) {
    printf("ERROR : %d, when attempting initial connection\n", err);
    connect_client(fd, flen);
  } else if (strncmp(buf, CONNECT, strlen(CONNECT)) != 0) {
    printf("Not a connect message!, skipping!\n");
    connect_client(fd, flen);
    return;
  } else {
    printf("Connected to host : %s, port : %d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
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

