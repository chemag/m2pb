// Copyright Google Inc. Apache 2.0.

#include "h264_utils.h"

#include <stdint.h>  // for uint8_t, int32_t, uint32_t, etc
#include <stdio.h>  // for NULL


static uint8_t *nal_start_search(const uint8_t *data, int size, int *type) {
	int zero_bytes = 0;
	const uint8_t *p = data;
	// start code prefix search
	while (p < data + size - 1) {
		if (*p == 0) zero_bytes++;
		if (zero_bytes >= 3 && *p) {
			if (*p != 1 && *p != 3) {
				// illegal byte codes in H.264
				p++;
				*type = -1;
				return (uint8_t *)p;
			} else if (*p == 1) {
				if (*(p + 1) & 0x80) {  // forbidden bits
					// illegal byte codes in H.264
					p++;
					*type = -1;
					return (uint8_t *)p;
				} else {
					p++;
					*type = 1;            // found NAL of 4-bytes SYNC
					return (uint8_t *)p;  // found HAL header
				}
			}
		} else if (zero_bytes == 2 && *p == 1) {
			p++;
			*type = 2;            // found NAL of 3-bytes NAL SYNC
			return (uint8_t *)p;  // found HAL header
		}
		if (*p) zero_bytes = 0;
		p++;
	}
	*type = 0;
	return NULL;
}

static const uint8_t *h264_picture_nal_search(const uint8_t *data, int size) {
	int type, nal_unit_type;
	const uint8_t *nal_start, *nal_data;
	nal_data = data;
	do {
		nal_start = nal_start_search(nal_data, size, &type);
		if (type == -1)  // invalid nal unit, it's not h.264 format
			return NULL;
		if (nal_start == NULL)  // nal not found
			return NULL;
		size -= nal_start - nal_data;
		nal_data = nal_start;
		if (nal_start != NULL) {
			nal_unit_type = *nal_start & 0x1f;
			if (nal_unit_type == 1 ||
					nal_unit_type == 5) {  // idr_slice or no_idr_slice
				return nal_start;
			}
		}
	} while (size > 4 || nal_data != NULL);
	return NULL;
}

typedef struct BITS_I {
	const uint8_t *buffer;
	int bits_offset;
	int total_bits;
	int error_flag;
} BITS_I;

static int golomb_code(const uint8_t buffer[], int totbitoffset, int *info,
					int maxbytes) {
	int inf;
	int32_t byteoffset;  // byte from start of buffer
	int bitoffset;       // bit from start of byte
	int ctr_bit = 0;     // control bit for current bit posision
	int bitcounter = 1;
	int len;
	int info_bit;

	byteoffset = totbitoffset >> 3;
	bitoffset = 7 - (totbitoffset & 0x07);
	ctr_bit = (buffer[byteoffset] & (0x01 << bitoffset));  // set up control bit

	len = 1;
	while (ctr_bit == 0) {
		// find leading 1 bit
		len++;
		bitoffset -= 1;
		bitcounter++;
		if (bitoffset < 0) {
			// finish with current byte ?
			bitoffset = bitoffset + 8;
			byteoffset++;
			if (byteoffset > maxbytes) return -1;
		}
		ctr_bit = buffer[byteoffset] & (0x01 << (bitoffset));
	}

	// make infoword
	inf = 0;  // shortest possible code is 1, then info is always 0
	for (info_bit = 0; (info_bit < (len - 1)); info_bit++) {
		bitcounter++;
		bitoffset -= 1;
		if (bitoffset < 0) {
			// finished with current byte ?
			bitoffset = bitoffset + 8;
			byteoffset++;
			if (byteoffset > maxbytes) return -1;
		}
		inf = (inf << 1);
		if (buffer[byteoffset] & (0x01 << (bitoffset))) inf |= 1;
	}

	*info = inf;
	return bitcounter;  // return absolute offset in bit from start of frame
}

static uint32_t ue(BITS_I *bits, int *code_bits) {
	int info;
	*code_bits =
			golomb_code(bits->buffer, bits->bits_offset, &info,
					(bits->total_bits >> 3) + ((bits->total_bits & 0x7) ? 1 : 0));
	if (*code_bits == -1) {
		bits->error_flag = 1;
		return 0;
	}
	return (1 << (*code_bits >> 1)) + info - 1;
}

static unsigned int read_ue(BITS_I *bits) {
	int val, bit_num;
	val = ue(bits, &bit_num);
	if (bits->error_flag) return 0;
	bits->bits_offset += bit_num;
	bits->total_bits -= bit_num;
	return val;
}

static int h264_picture_type_parse(const uint8_t *data, int size) {
	int slice_type;
	struct BITS_I bits;
	if (size <= 0 || data == NULL) return H264_FRAME_TYPE_UNKNOWN;
	// because slice type is in the first bytes of a nal, which can't be zero, we
	// skip doing
	// nal_rbsp to remove startcode prevention code.
	bits.buffer =
			data;  // we need to decode nal to rbsp, the first 2 fields of slice;
	bits.bits_offset = 0;
	bits.total_bits = size * 8;
	read_ue(&bits);  // skip first_mb_in_slice
	slice_type = read_ue(&bits);
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

int h264_frame_type(const uint8_t *data, int len) {
	if (len < 7) return H264_FRAME_TYPE_UNKNOWN;

	uint64_t code = 0xffffffffffffff00 | *data++;
	while (--len) {
		// TODO(chema): use ffmpeg code for this (?)
		// Quick method: find the Access Unit Delimiter NALU (nal_unit_type=9).
		// It has a one byte RBSP (ue(3) primary_picture_type + '1' + [5]
		// trailing zero bits). The primary_picture_type gives use the
		// frame_type.
		// If this fails, find a NAL with a slice header and use the slice
		// type to derive the frame type.
		if (((code & 0x00ffffffffffffff) == 0x0000000001091000LL)) {
			// primary_picture_type = 0 -> I slices
			return H264_FRAME_TYPE_I;
		} else if (((code & 0x00ffffffffffffff) == 0x0000000001093000LL)) {
			// primary_picture_type = 1 -> P, I slices
			return H264_FRAME_TYPE_P;
		} else if (((code & 0x00ffffffffffffff) == 0x0000000001095000LL)) {
			// primary_picture_type = 2 -> B, P, I slices
			return H264_FRAME_TYPE_B;
		} else if (((code & 0x00ffffffffff0000) == 0x0000000001090000LL)) {
			// we can't determine frametype by access unit delimiter.
			const uint8_t *nal;
			nal = h264_picture_nal_search(data, len);
			if (nal != NULL) {
				int picture_type =
					h264_picture_type_parse(nal + 1, len - (int)(nal - data) - 1);
				return picture_type;
			}
			return H264_FRAME_TYPE_UNKNOWN;
		}
		code = ((code << 8) | *data++);
	}
	return H264_FRAME_TYPE_UNKNOWN;
}
