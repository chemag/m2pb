#include <arpa/inet.h>
#include <ctype.h>
#include <getopt.h>
#include <inttypes.h>  // for PRId64
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

// sagesrv -> miniclient commands
#define MEDIACMD_GETMEDIATIME 17
#define MEDIACMD_FLUSH 22
#define MEDIACMD_PUSHBUFFER 23
#define MEDIACMD_DVD_NEWCELL_NOREPLY 41

#define UINT32(x)                                                    \
  (((uint32_t) * ((x) + 0) << 24) | ((uint32_t) * ((x) + 1) << 16) | \
   ((uint32_t) * ((x) + 2) << 8) | ((uint32_t) * ((x) + 3)))

int sagesrv_interpret_buffer(uint8_t *buffer, int len, int debug,
                             int64_t *pts_delta) {
  int mediacmd, mediasubcmd;
  int i = 0;

  while ((i + 8) <= len) {
    // read the command&subcommand bytes
    mediacmd = buffer[i + 0];
    mediasubcmd = buffer[i + 4];
    // StreamerChannel::Flush()
    if ((mediacmd == MEDIACMD_GETMEDIATIME) &&
        (mediasubcmd == MEDIACMD_FLUSH)) {
      i += 8;
      if (debug > 0) printf("found StreamerChannel::Flush()\n");
      continue;
    }
    // StreamerChannel::SendPushbufferHeader()
    // StreamerChannel::PushFilePostSwitch()
    if ((mediacmd == MEDIACMD_GETMEDIATIME) &&
        (mediasubcmd == MEDIACMD_PUSHBUFFER)) {
      i += 16;
      if (debug > 0) printf("found StreamerChannel::SendPushbufferHeader()\n");
      continue;
    }
    if (mediacmd == MEDIACMD_DVD_NEWCELL_NOREPLY) {
      // get the length
      uint32_t plen = UINT32(buffer + i + 4);
      // reverse StreamerChannel::SetClientTsRemapParams()
      uint32_t word = UINT32(buffer + i + 8);
      *pts_delta = word << 1;
      printf("found new pts_delta: %li\n", *pts_delta);
      i += 8 + plen;
      continue;
    }
    return -1;
  }
  if (i == len) {
    return 0;
  }
  return -1;
}
