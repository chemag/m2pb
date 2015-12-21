// Copyright Google Inc. Apache 2.0.

#include <stdint.h>  // for uint8_t


int ac3_syncframe_distance(const uint8_t *data, int len) {
  // syncframe_distance = ac3_syncframe_distance(data, len);
  for (int i = 0; i < len - 5; ++i) {
    if (data[i] == 0x0b && data[i+1] == 0x77 &&  // syncword = 0x0b66
        (data[i+4] == 0x14 ||  // fscod/frmsizecode = 0x14 for 48 kHz, 192 kbps
         data[i+4] == 0x0c))   // fscod/frmsizecode = 0x0c for 48 kHz, 96 kbps
      return i;
  }
  return -1;
}
