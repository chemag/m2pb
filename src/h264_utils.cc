// Copyright Google Inc. Apache 2.0.

#include "h264_utils.h"

#include <stdint.h>  // for uint8_t, int32_t, uint32_t, etc
#include <stdio.h>  // for NULL

#include "bitstream.h"


static int get_type_from_slice_header(const uint8_t *data, int size) {
  if (size <= 0 || data == NULL)
    return H264_FRAME_TYPE_UNKNOWN;

  // because slice_type is in the first bytes of the NAL, which can't be
  // zero, we don't need to filter startcode prevention bytes.
	Bitstream bits(data, size);
	// skip first_mb_in_slice
	uint32_t out;
	if (!bits.ReadGolombUint32(&out))
    return H264_FRAME_TYPE_UNKNOWN;
  uint32_t slice_type;
	if (!bits.ReadGolombUint32(&slice_type))
    return H264_FRAME_TYPE_UNKNOWN;

  if (slice_type == 0 || slice_type == 3 || slice_type == 5 ||
      slice_type == 8) {
    // frame type p
    return H264_FRAME_TYPE_P;
  }
  if (slice_type == 1 || slice_type == 6) {
    // frame type b
    return H264_FRAME_TYPE_B;
  }
  if (slice_type == 2 || slice_type == 4 || slice_type == 7 ||
      slice_type == 9) {
    // frame type I
    return H264_FRAME_TYPE_I;
  }
  return H264_FRAME_TYPE_OTHER;
}


int h264_frame_type(const uint8_t *buffer, int len) {
	if (len < 7) return H264_FRAME_TYPE_UNKNOWN;

	// 3 ways to get the frame type:
	// 1. Find Access Unit Delimiter NALU and extract its primary_picture_type
	//   Specifically, we look for the following 8-byte pattern:
	//   4-byte start_code (0x00000001)
	//   1-byte NALU header (0x09)
	//     [1]: forbidden_zero_bit (0b0)
	//     [2]: nal_ref_idc (0b00)
	//     [5]: nal_unit_type (0x9 for Access Unit Delimiter)
	//   1-Byte access_unit_delimiter_rbsp (0x10, 0x30, 0x50)
	//     [3]: primary_picture_type (I:0, 1:P, 2:B)
	//     [5]: rbsp_trailing_bits (0b10000)
	//   1-byte zero_byte (0x00)
	//
	// 2. Find an IDR picture slice NAL
	//   3-byte start_code (0x000001)
	//   1-byte NALU header (0x05)
	//
	// 3. Find a non-IDR picture slice NAL and get the slice type
	//   3-byte start_code (0x000001)
	//   1-byte NALU header (0x01)
	//   [variable]: first_mb_in_slice (skip)
	//   [variable]: slice_type

	const uint8_t *data = buffer;
	uint64_t code = 0xffffffff;
	// read first 4 bytes
	for (int i = 0; i < 4 && i < len; i++)
		code = ((code << 8) | *data++);

	for (int i = 4; i < len; i++) {
		code = (code << 8) | *data++;
		if ((code & 0x00ffffffffffffff) == 0x0000000001091000LL) {
			// nalu, primary_picture_type = 0 -> I slices
			return H264_FRAME_TYPE_I;
		} else if ((code & 0x00ffffffffffffff) == 0x0000000001093000LL) {
			// nalu, primary_picture_type = 1 -> P and I slices
			return H264_FRAME_TYPE_P;
		} else if ((code & 0x00ffffffffffffff) == 0x0000000001095000LL) {
			// nalu, primary_picture_type = 2 -> B, P and I slices
			return H264_FRAME_TYPE_B;
		} else if ((code & 0x000000ffffffff00) == 0x0000000000010500LL) {
			// IDR picture slice
			return H264_FRAME_TYPE_I;
		} else if ((code & 0x000000ffffffff00) == 0x0000000000010100LL) {
			// Non-IDR picture slice. Parse the slice header to get its type
			data--;
			int bytes_left = len - (int)(data - buffer);
			return get_type_from_slice_header(data, bytes_left);
		}
	}
	return H264_FRAME_TYPE_UNKNOWN;
}
