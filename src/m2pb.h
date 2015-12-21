// Copyright Google Inc. Apache 2.0.

#ifndef M2PB_H_
#define M2PB_H_

#define MPEG_TS_PACKET_SYNC 0x47  // 'G'
#define MPEG_TS_PACKET_SIZE 188
#define MPEG_TS_SYNC_IN_A_ROW 3

// synchronization parameters
#define DEFAULT_MAXIMUM_SYNC_GAP (10 * MPEG_TS_PACKET_SIZE)
#define SYNC_GAP_MINIMUM MPEG_TS_PACKET_SIZE
#define SYNC_GAP_MAXIMUM (100 * MPEG_TS_PACKET_SIZE)

#endif  // M2PB_H_
