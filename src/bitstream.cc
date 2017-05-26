#include "bitstream.h"

#include <stdint.h>  // for uint8_t, uint32_t, int32_t, etc


Bitstream::Bitstream(const uint8_t *buffer, int size_in_bytes) {
  buffer_ = buffer;
  bits_left_ = size_in_bytes * 8;
  bits_offset_ = 0;
}


bool Bitstream::ReadUint32(int bits_to_read, uint32_t *out) {
  if ((bits_to_read > 32) || (bits_left_ < bits_to_read)) {
    return false;
  }

  int32_t byte_offset = bits_offset_ >> 3;
  int bit_loc = 7 - (bits_offset_ & 0x07);

  *out = 0;
  for (int i = 0; i < bits_to_read; i++) {
    *out = (*out << 1) | ((buffer_[byte_offset] >> bit_loc) & 1);
    if (--bit_loc < 0) {
      bit_loc = 7;
      byte_offset++;
    }
  }
  bits_left_ -= bits_to_read;
  bits_offset_ += bits_to_read;
  return true;
}

// Skips the given number of bits in the bitstream
bool Bitstream::Skip(int bits_to_skip) {
  if (bits_left_ < bits_to_skip) {
    return false;
  }
  bits_left_ -= bits_to_skip;
  bits_offset_ += bits_to_skip;
  return true;
}


bool Bitstream::ReadGolombUint32(uint32_t *out) {
  // From H.264 spec "9.1 Parsing process for Exp-Golomb codes":
  // Bit string format of golomb codes:
  //
  //    Bit string form      Range of codes
  //           1                   0
  //         0 1 x0               1..2
  //       0 0 1 x1 x0            3..6
  //     0 0 0 1 x2 x1 x0         7..14
  //   0 0 0 0 1 x3 x2 x1 x0     15..30
  // 0 0 0 0 0 1 x4 x3 x2 x1 x0  31..62
  //          ...
  //
  // Decode logic:
  //   leadingZeroBits = -1
  //   for( b = 0; !b; leadingZeroBits++ )
  //     b = read_bits( 1 )
  //   code = 2^leadingZeroBits - 1 + read_bits( leadingZeroBits )

  // set location/offset
  int32_t byte_offset = bits_offset_ >> 3;
  int bit_loc = 7 - (bits_offset_ & 0x07);

  int leading_zero_bits = -1;
  for (int b = 0; !b && bits_left_ > 0; leading_zero_bits++) {
    // b = read_bits( 1 );
    b = (buffer_[byte_offset] >> bit_loc) & 1;
    if (--bit_loc < 0) {
      bit_loc = 7;
      byte_offset++;
    }
    bits_left_--;
    bits_offset_++;
  }

  // The same number of zero bits follows after the '1' bit. E.g.: 0 0 1 x1 x0
  if (!bits_left_ || bits_left_ < leading_zero_bits) {
    return false;
  }

  uint32_t suffix_bits;
  if (!ReadUint32(leading_zero_bits, &suffix_bits))
    return false;
  *out = (1 << leading_zero_bits) - 1 + suffix_bits;
  return true;
}


bool Bitstream::ReadGolombInt32(int32_t *out) {
  // read unsigned value
  uint32_t u_out = 0;
  if (!ReadGolombUint32(&u_out))
    return false;

  // convert into signed
  *out = (u_out + 1) >> 1;
  if ((u_out & 0x01) == 0)  // lsb is signed bit
    *out = -*out;
  return true;
}
