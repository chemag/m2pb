// Copyright Google Inc. Apache 2.0.

#ifndef H264_UTILS_H_
#define H264_UTILS_H_

#include <stdint.h>  // for uint8_t

#define H264_FRAME_TYPE_UNKNOWN 0
#define H264_FRAME_TYPE_I 1
#define H264_FRAME_TYPE_P 2
#define H264_FRAME_TYPE_B 3
#define H264_FRAME_TYPE_OTHER 4

// Returns the h.264 frame type of a buffer.
int h264_frame_type(const uint8_t *data, int len);

#endif  // H264_UTILS_H_
