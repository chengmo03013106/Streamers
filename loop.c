#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <net_helper.h>
#include <topmanager.h>

#include "streaming.h"
#include "loop.h"

#define BUFFSIZE 64 * 1024
static struct timeval period = {0, 500000};
static struct timeval tnext;

void tout_init(struct timeval *tv)
{
  struct timeval tnow;

  if (tnext.tv_sec == 0) {
    gettimeofday(&tnext, NULL);
  }
  gettimeofday(&tnow, NULL);
  if(timercmp(&tnow, &tnext, <)) {
    timersub(&tnext, &tnow, tv);
  } else {
    *tv = (struct timeval){0, 0};
  }
}

static int wait4data(struct nodeID *s)
{
  fd_set fds;
  int res;
  struct timeval tv;
  int fd = getFD(s);

  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  tout_init(&tv);
  res = select(fd + 1, &fds, NULL, NULL, &tv);
  if (FD_ISSET(fd, &fds)) {
    return fd;
  }

  return -1;
}

void loop(struct nodeID *s, int csize, int buff_size)
{
  int done = 0;
  static uint8_t buff[BUFFSIZE];
  int cnt = 0;
  
  period.tv_sec = csize / 1000000;
  period.tv_usec = csize % 1000000;
  
  topParseData(NULL, 0);
  stream_init(buff_size, s);
  while (!done) {
    int len;
    int fd;

    fd = wait4data(s);
    if (fd > 0) {
      struct nodeID *remote;

      len = recv_data(s, &remote, buff, BUFFSIZE);
      switch (buff[0] /* Message Type */) {
        case 0x10 /* NCAST_PROTO */:
          topParseData(buff, len);
          break;
        case 12:
          received_chunk(buff, len);
          break;
        default:
          fprintf(stderr, "Unknown Message Type %x\n", buff[0]);
      }
      free(remote);
    } else {
      const struct nodeID **neighbours;
      int n;
      struct timeval tmp;

      neighbours = topGetNeighbourhood(&n);
      send_chunk(neighbours, n);
      if (cnt++ % 10 == 0) {
        topParseData(NULL, 0);
      }
      timeradd(&tnext, &period, &tmp);
      tnext = tmp;
    }
  }
}

void source_loop(const char *fname, struct nodeID *s, int csize, int chunks)
{
  int done = 0;
  static uint8_t buff[BUFFSIZE];
  int cnt = 0;

  period.tv_sec = csize  / 1000000;
  period.tv_usec = csize % 1000000;
  
  source_init(fname, s);
  while (!done) {
    int len;
    int fd;

    fd = wait4data(s);
    if (fd > 0) {
      struct nodeID *remote;

      len = recv_data(s, &remote, buff, BUFFSIZE);
      switch (buff[0] /* Message Type */) {
        case 0x10 /* NCAST_PROTO */:
          fprintf(stderr, "Top Parse\n");
          topParseData(buff, len);
          break;
        default:
          fprintf(stderr, "Bad Message Type %x\n", buff[0]);
      }
      free(remote);
    } else {
      const struct nodeID **neighbours;
      int i, n;
      struct timeval tmp;

      generated_chunk();
      neighbours = topGetNeighbourhood(&n);
      for (i = 0; i < chunks; i++) {
        send_chunk(neighbours, n);
      }
      if (cnt++ % 10 == 0) {
        topParseData(NULL, 0);
      }
      timeradd(&tnext, &period, &tmp);
      tnext = tmp;
    }
  }
}
