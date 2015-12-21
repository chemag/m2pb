// Copyright Google Inc. Apache 2.0.

#include "mpeg2ts_reader.h"

#include <inttypes.h>  // for PRId64
#include <string.h>  // for memmove


int Mpeg2TsReader::GetChunk(uint8_t **buf, int64_t *pi, int64_t *bi) {
  // store current status
  *buf = buffer_;
  *pi = pi_;
  *bi = bi_;

  // try to get at least 1 block
  if (blen_ < MPEG_TS_PACKET_SIZE) {
    size_t inbytes = fread(buffer_ + blen_, 1, MPEG_TS_PACKET_SIZE - blen_,
        fin_);
    if (debug_ > 3) {
      printf("%" PRId64 "-%" PRId64 ": reading %i\n",
          bi_, bi_ + inbytes, MPEG_TS_PACKET_SIZE - blen_);
    }
    blen_ += inbytes;
  }
  // ensure we have 1 block available
  if (blen_ < MPEG_TS_PACKET_SIZE && feof(fin_)) {
    // treat this as a full chunk
    return blen_;
  }

  // check whether this is a valid packet
  if (blen_ >= MPEG_TS_PACKET_SIZE && buffer_[0] == MPEG_TS_PACKET_SYNC) {
    if (debug_ > 2) {
      printf("%" PRId64 ": found 0x47\n", bi_);
    }
    return MPEG_TS_PACKET_SIZE;
  }

  // need to sync the stream

  // read (up to) sync_gap bytes
  if (blen_ < sync_gap_) {
    size_t inbytes = fread(buffer_ + blen_, 1, sync_gap_ - blen_, fin_);
    if (debug_ > 1) {
      printf("%" PRId64 "-%" PRId64 ": reading %i\n", bi_, bi_ + inbytes,
          sync_gap_ - blen_);
    }
    blen_ += inbytes;
  }

  // ensure we have enough bytes to sync up
  if (blen_ < (3*MPEG_TS_PACKET_SIZE) && feof(fin_))
    return blen_;

  // look for 3 'G's in a row, up to MPEG_TS_PACKET_SIZE bytes from the
  // beginning
  int i = 0;
  while ((2*i+MPEG_TS_PACKET_SIZE) < blen_) {
    if (buffer_[i] == MPEG_TS_PACKET_SYNC &&
        buffer_[i+MPEG_TS_PACKET_SIZE] == MPEG_TS_PACKET_SYNC &&
        buffer_[i+(2*MPEG_TS_PACKET_SIZE)] == MPEG_TS_PACKET_SYNC) {
      // found sync point
      if (debug_ > 2) {
        printf("%" PRId64 "-%" PRId64 "-%" PRId64 ": found 3x 0x47...\n",
            bi_ + i,
            bi_ + i + MPEG_TS_PACKET_SIZE,
            bi_ + i + (2*MPEG_TS_PACKET_SIZE));
      }
      // return the unsync'ed bytes
      return i;
    }
    ++i;
  }

  // no sync found: punt
  return -1;
}


void Mpeg2TsReader::Next(int used_size) {
  if (blen_ > used_size) {
    memmove(buffer_, buffer_ + used_size, blen_ - used_size);
  }
  blen_ -= used_size;
  bi_ += used_size;
  pi_ += 1;
}
