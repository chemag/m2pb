#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <stdint.h>  // for uint8_t, uint32_t, int32_t, etc

//
// Input bits - reads from a bitstream buffer
//
class Bitstream {
 public:
  Bitstream(const uint8_t *buffer, int size_in_bytes);

  // Skips the given number of bits in the bitstream
  bool Skip(int bits);

  // Reads the given number of bits from the bitstream and returns them
  // as a uint32 number
  bool ReadUint32(int bits_to_read, uint32_t *out);

  // Reads the next Golomb code word and returns it as a uint32. If an
  // error is hit returns false
  bool ReadGolombUint32(uint32_t *out);

  // Reads the next Exp-Golomb code word from the bitstream and returns it
  // as an int32_t. If an error is hit returns false
  bool ReadGolombInt32(int32_t *out);

  const uint8_t *buffer_;
  int bits_offset_;
  int bits_left_;
};

#endif /* BITSTREAM_H */
