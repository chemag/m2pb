// Copyright Google Inc. Apache 2.0.

#ifndef MPEG2TS_READER_H_
#define MPEG2TS_READER_H_

#include <stdint.h>  // for uint8_t, int64_t
#include <stdio.h>   // for FILE

#define MPEG_TS_PACKET_SYNC 0x47  // 'G'
#define MPEG_TS_PACKET_SIZE 188

#define DEFAULT_SYNC_GAP (10 * MPEG_TS_PACKET_SIZE)

// an mpeg-ts stream sync'er
//
// Algorithm:
//   - Find 'G', look 188 bytes down, expect another 'G'.
//   - If we find 3 'G's in a row, we found a sync point.
//     - If we do not find them in sync_gap_, punt.

class Mpeg2TsReader {
 public:
  explicit Mpeg2TsReader(FILE *fin, int debug)
      : fin_(fin), debug_(debug), sync_gap_(DEFAULT_SYNC_GAP) {
    buffer_ = new uint8_t[sync_gap_];
    blen_ = 0;
    pi_ = 0;
    bi_ = 0;
  }
  ~Mpeg2TsReader() { delete[] buffer_; }

  int SetSyncGap(int sync_gap) {
    delete[] buffer_;
    sync_gap_ = sync_gap;
    buffer_ = new uint8_t[sync_gap_];
    blen_ = 0;
    if (!buffer_) {
      return -1;
    }
    return 0;
  }

  // Get a chunk (a packet, or a non-parseable chunk)
  int GetChunk(uint8_t **buf, int64_t *pi, int64_t *bi);

  // Get next packet
  void Next(int used_size);

 private:
  FILE *fin_;
  int debug_;
  int sync_gap_;
  int64_t bi_;
  int64_t pi_;
  uint8_t *buffer_;
  int blen_;
};

#endif  // MPEG2TS_READER_H_
