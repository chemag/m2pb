// Copyright Google Inc. Apache 2.0.

#include "mpeg2ts_parser.h"

#include <string.h>

#include <algorithm>

#include "mpeg2ts.pb.h"


// set <len> bits at <bitindex> position in buf using the
// <len> right-most (least-significant) bits in <val>
static void BitSet(uint8_t *buf, int bitindex, int len, int32_t val) {
  uint8_t mask;
  len += bitindex;
  // set one byte at a time
  while (bitindex < len) {
    // we start at bitindex % 8
    int offset = bitindex % 8;
    // length printed now
    int nowlen = std::min(8 - offset, len - bitindex);
    // get the byte to print
    uint8_t *byte = (uint8_t *)(buf + bitindex / 8);
    // get the mask
    mask = 0xff >> offset;
    if ((len - bitindex) < 8)
      mask &= ~(0xff >> (offset + nowlen));
    *byte &= ~mask;
    // put the value in the right position
    int rshift = (len - bitindex + offset - 8);
    uint8_t valbyte = 0;
    if (rshift >= 0)
      valbyte = (val >> rshift);
    else
      valbyte = (val << -rshift);
    // mask it
    *byte |= valbyte & mask;
    // next
    bitindex += nowlen;
  }
}

// returns val[first:last] (inclusive)
static int64_t BitGet(int64_t val, int first, int last) {
  int64_t res = val >> first;
  return res & ((1 << (last - first + 1)) - 1);
}


Mpeg2TsParser::Mpeg2TsParser(
    bool return_raw_packets)
    : return_raw_packets_(return_raw_packets) {}


int Mpeg2TsParser::ParsePacket(int64_t pi, int64_t bi,
    const uint8_t *buf, int len, Mpeg2Ts *mpeg2ts) {
  // clear input
  mpeg2ts->Clear();
  mpeg2ts->set_packet(pi);
  mpeg2ts->set_byte(bi);
  int res = ParseValidPacket(buf, len, mpeg2ts->mutable_parsed());
  if (res < 0) {
    mpeg2ts->clear_parsed();
    mpeg2ts->set_raw(buf, len);
    return len;
  }
  return res;
}

int Mpeg2TsParser::DumpPacket(const Mpeg2Ts &mpeg2ts, uint8_t *buf, int len) {
  int bi = 0;
  int res;
  if (mpeg2ts.has_parsed()) {
    return DumpValidPacket(mpeg2ts.parsed(), buf, len);
  } else if (mpeg2ts.has_raw()) {
    // dump raw packet
    if (mpeg2ts.raw().length() > (unsigned int)len)
      // not enough space for the raw packet
      return -1;
    res = mpeg2ts.raw().copy((char *)(buf + bi), len - bi);
    bi += res;
    return bi;
  }
  // error
  return -1;
}


int Mpeg2TsParser::ParseValidPacket(const uint8_t *buf, int len,
    Mpeg2TsPacket *mpeg2ts_packet) {
  int bi = 0;
  int res;
  // reset output
  mpeg2ts_packet->Clear();
  // parse mpeg2-ts header
  res = ParseHeader(buf + bi, len - bi, mpeg2ts_packet->mutable_header());
  if (res < 0) {
    return -1;
  }
  bi += res;

#if 0
  if (mpeg2ts_packet.pid() == 0x1fff) {
    // null packet: ignore it
    if (status->debug > 0) {
      printf("-- null packet (pid: 0x1fff)\n");
    }
    return len;
  }
#endif

  // adaptation field
  if (mpeg2ts_packet->header().adaptation_field_exists()) {
    res = ParseAdaptationField(buf + bi, len - bi,
        mpeg2ts_packet->mutable_adaptation_field());
    if (res < 0) {
      return -1;
    }
    bi += res;
  }

  if (mpeg2ts_packet->header().payload_unit_start_indicator()) {
    // check PES/PSI packet
    bool is_pes = false;
    if (buf[bi+0] == 0 && buf[bi+1] == 0 && buf[bi+2] == 1)
      is_pes = true;

    if (is_pes) {
      res = ParsePesPacket(buf + bi, len - bi,
          mpeg2ts_packet->mutable_pes_packet());
      if (res < 0) {
        return -1;
      }
      bi += res;
    } else {
      res = ParsePsiPacket(buf + bi, len - bi,
          mpeg2ts_packet->mutable_psi_packet());
      if (res < 0) {
        return -1;
      }
      bi += res;
    }
  }

  // remainder is data bytes
  if (len - bi > 0) {
    mpeg2ts_packet->set_data_bytes(buf + bi, len - bi);
    bi = len;
  }

  return bi;
}


int Mpeg2TsParser::DumpValidPacket(const Mpeg2TsPacket &mpeg2ts_packet,
    uint8_t *buf, int len) {
  int bi = 0;
  int res;

  // ensure enough space for a valid packet
  if (len < MPEG_TS_PACKET_SIZE)
    return -1;

  // dump the mpeg2-ts header
  res = DumpHeader(mpeg2ts_packet.header(), buf + bi, len - bi);
  if (res < 0) {
    return -1;
  }
  bi += res;

  // dump the adaptation field
  if (mpeg2ts_packet.header().adaptation_field_exists()) {
    res = DumpAdaptationField(mpeg2ts_packet.adaptation_field(),
        buf + bi, len - bi);
    if (res < 0) {
      return -1;
    }
    bi += res;
  }

  // dump the payload
  if (mpeg2ts_packet.header().payload_unit_start_indicator()) {
    // check PES/PSI packet
    if (mpeg2ts_packet.has_pes_packet()) {
      res = DumpPesPacket(mpeg2ts_packet.pes_packet(), buf + bi, len - bi);
      if (res < 0) {
        return -1;
      }
      bi += res;
    } else if (mpeg2ts_packet.has_psi_packet()) {
      res = DumpPsiPacket(mpeg2ts_packet.psi_packet(), buf + bi, len - bi);
      if (res < 0) {
        return -1;
      }
      bi += res;
    } else {
      return -1;
    }
	}

  if (mpeg2ts_packet.has_data_bytes()) {
    if (mpeg2ts_packet.data_bytes().length() > (unsigned int)len)
      // not enough space for the data bytes
      return -1;
    res = mpeg2ts_packet.data_bytes().copy((char *)(buf + bi), len - bi);
    bi += res;
  }

  return bi;
}


int Mpeg2TsParser::ParseHeader(const uint8_t *buf, int len,
    Mpeg2TsHeader *mpeg2ts_header) {
  // ensure ts header can be parsed
  if (len < 4)
    return -1;
  // parse ts header
  if (buf[0] != MPEG_TS_PACKET_SYNC)
    return -1;
  // flags
  int transport_error_indicator = (buf[1] & 0x80) >> 7;
  mpeg2ts_header->set_transport_error_indicator(transport_error_indicator);
  int payload_unit_start_indicator = (buf[1] & 0x40) >> 6;
  mpeg2ts_header->set_payload_unit_start_indicator(
      payload_unit_start_indicator);
  int transport_priority = (buf[1] & 0x20) >> 5;
  mpeg2ts_header->set_transport_priority(transport_priority);
  uint32_t pid = ((buf[1] & 0x1f) << 8) | buf[2];
  mpeg2ts_header->set_pid(pid);
  int transport_scrambling_control = (buf[3] & 0xc0) >> 6;
  mpeg2ts_header->set_transport_scrambling_control(
      transport_scrambling_control);
  int adaptation_field_exists = (buf[3] & 0x20) >> 5;
  mpeg2ts_header->set_adaptation_field_exists(adaptation_field_exists);
  int payload_exists = (buf[3] & 0x10) >> 4;
  mpeg2ts_header->set_payload_exists(payload_exists);
  int continuity_counter = (buf[3] & 0x0f);
  mpeg2ts_header->set_continuity_counter(continuity_counter);
  return 4;
}


int Mpeg2TsParser::DumpHeader(const Mpeg2TsHeader &mpeg2ts_header,
    uint8_t *buf, int len) {
  int bi = 0;

  // ensure ts header can be dumped
  if (len < 4)
    return -1;
  // dump ts header
  buf[bi] = MPEG_TS_PACKET_SYNC;
  bi += 1;
  // flags
  if (!mpeg2ts_header.has_transport_error_indicator())
    return -1;
  BitSet(buf+bi, 0, 1, mpeg2ts_header.transport_error_indicator());
  if (!mpeg2ts_header.has_payload_unit_start_indicator())
    return -1;
  BitSet(buf+bi, 1, 1, mpeg2ts_header.payload_unit_start_indicator());
  if (!mpeg2ts_header.has_transport_priority())
    return -1;
  BitSet(buf+bi, 2, 1, mpeg2ts_header.transport_priority());
  if (!mpeg2ts_header.has_pid())
    return -1;
  BitSet(buf+bi, 3, 13, mpeg2ts_header.pid());
  if (!mpeg2ts_header.has_transport_scrambling_control())
    return -1;
  bi += 2;
  BitSet(buf+bi, 0, 2, mpeg2ts_header.transport_scrambling_control());
  if (!mpeg2ts_header.has_adaptation_field_exists())
    return -1;
  BitSet(buf+bi, 2, 1, mpeg2ts_header.adaptation_field_exists());
  if (!mpeg2ts_header.has_payload_exists())
    return -1;
  BitSet(buf+bi, 3, 1, mpeg2ts_header.payload_exists());
  if (!mpeg2ts_header.has_continuity_counter())
    return -1;
  BitSet(buf+bi, 4, 4, mpeg2ts_header.continuity_counter());
  bi += 1;
  return bi;
}


int Mpeg2TsParser::ParseAdaptationField(const uint8_t *buf, int len,
    AdaptationField *adaptation_field) {
  int bi = 0;
  int res;
  // ensure adaptation field can be parsed
  if (len < 1)
    return -1;
  int adaptation_field_length = buf[bi];
  adaptation_field->set_adaptation_field_length(adaptation_field_length);
  bi += 1;
  // ensure adaptation field can be parsed
  if (len < (bi + adaptation_field_length))
    return -1;
  if (adaptation_field_length == 0)
    return bi;
  // get flags
  int discontinuity_indicator = (buf[bi] & 0x80) >> 7;
  adaptation_field->set_discontinuity_indicator(discontinuity_indicator);
  int random_access_indicator = (buf[bi] & 0x40) >> 6;
  adaptation_field->set_random_access_indicator(random_access_indicator);
  int elementary_stream_priority_indicator = (buf[bi] & 0x20) >> 5;
  adaptation_field->set_elementary_stream_priority_indicator(
      elementary_stream_priority_indicator);
  int pcr_flag = (buf[bi] & 0x10) >> 4;
  int opcr_flag = (buf[bi] & 0x08) >> 3;
  int splicing_point_flag = (buf[bi] & 0x04) >> 2;
  adaptation_field->set_splicing_point_flag(splicing_point_flag);
  int transport_private_data_flag = (buf[bi] & 0x02) >> 1;
  int adaptation_field_extension_flag = (buf[bi] & 0x01);
  bi += 1;

  if (pcr_flag) {
    res = ParsePCR(buf + bi, len - bi, adaptation_field->mutable_pcr());
    if (res < 0)
      return -1;
    bi += res;
  }

  if (opcr_flag) {
    res = ParsePCR(buf + bi, len - bi, adaptation_field->mutable_opcr());
    if (res < 0)
      return -1;
    bi += res;
  }

  if (splicing_point_flag) {
    int splice_countdown = buf[bi];
    adaptation_field->set_splice_countdown(splice_countdown);
    bi += 1;
  }

  if (transport_private_data_flag) {
    int private_data_len = buf[bi];
    bi += 1;
    adaptation_field->set_transport_private_data(buf + bi, private_data_len);
    bi += private_data_len;
  }

  if (adaptation_field_extension_flag) {
    res = ParseAdaptationFieldExtension(buf + bi, len - bi,
        adaptation_field->mutable_adaptation_field_extension());
    if (res < 0)
      return -1;
    bi += res;
  }

  // skip stuffing bytes
  bi = 1 + adaptation_field_length;

  return bi;
}


int Mpeg2TsParser::DumpAdaptationField(const AdaptationField &adaptation_field,
    uint8_t *buf, int len) {
  int bi = 0;
  int res;

  buf[bi] = adaptation_field.adaptation_field_length();
  bi += 1;
  if (adaptation_field.adaptation_field_length() == 0)
    return bi;

  // discontinuity_indicator
  BitSet(buf+bi, 0, 1, adaptation_field.discontinuity_indicator());
  // random_access_indicator
  BitSet(buf+bi, 1, 1, adaptation_field.random_access_indicator());
  // elementary_stream_priority_indicator
  BitSet(buf+bi, 2, 1, adaptation_field.elementary_stream_priority_indicator());
  // pcr_flag
  BitSet(buf+bi, 3, 1, adaptation_field.has_pcr());
  // opcr_flag
  BitSet(buf+bi, 4, 1, adaptation_field.has_opcr());
  // splicing_point_flag
  BitSet(buf+bi, 5, 1, adaptation_field.splicing_point_flag());
  // transport_private_data_flag
  BitSet(buf+bi, 6, 1, adaptation_field.has_transport_private_data());
  // adaptation_field_extension_flag
  BitSet(buf+bi, 7, 1, adaptation_field.has_adaptation_field_extension());
  bi += 1;

  if (adaptation_field.has_pcr()) {
    res = DumpPCR(adaptation_field.pcr(), buf + bi, len - bi);
    if (res < 0)
      return -1;
    bi += res;
  }

  if (adaptation_field.has_opcr()) {
    res = DumpPCR(adaptation_field.opcr(), buf + bi, len - bi);
    if (res < 0)
      return -1;
    bi += res;
  }

  if (adaptation_field.splicing_point_flag()) {
    buf[bi] = adaptation_field.splice_countdown();
    bi += 1;
  }

  if (adaptation_field.has_transport_private_data()) {
    buf[bi] = adaptation_field.transport_private_data().length();
    bi += 1;
    res = adaptation_field.transport_private_data().copy(
        (char *)(buf + bi), len - bi);
    bi += res;
  }

  if (adaptation_field.has_adaptation_field_extension()) {
    res = DumpAdaptationFieldExtension(
        adaptation_field.adaptation_field_extension(),
        buf + bi, len - bi);
    if (res < 0)
      return -1;
    bi += res;
  }

  // add stuffing bytes
  while (bi < 1 + adaptation_field.adaptation_field_length()) {
    buf[bi++] = 0xff;
  }
  return bi;
}


int Mpeg2TsParser::ParseAdaptationFieldExtension(const uint8_t *buf, int len,
    AdaptationFieldExtension *adaptation_field_extension) {
  int bi = 0;
  // ensure adaptation field can be parsed
  if (len < 1)
    return -1;
  int adaptation_field_extension_length = buf[bi];
  adaptation_field_extension->set_adaptation_field_extension_length(
      adaptation_field_extension_length);
  bi += 1;
  // ensure adaptation field can be parsed
  if (len < (bi + adaptation_field_extension_length))
    return -1;
  int ltw_flag = (buf[bi] & 0x80) >> 7;
  int piecewise_rate_flag = (buf[bi] & 0x40) >> 6;
  int seamless_splice_flag = (buf[bi] & 0x20) >> 5;
  bi += 1;
  if (ltw_flag) {
    int ltw_valid_flag = (buf[bi] & 0x80) >> 7;
    adaptation_field_extension->set_ltw_valid_flag(ltw_valid_flag);
    int ltw_offset = ((int)(buf[bi] & 0x7f) << 8) | ((int)(buf[bi+1]));
    adaptation_field_extension->set_ltw_offset(ltw_offset);
    bi += 2;
  }
  if (piecewise_rate_flag) {
    int piecewise_rate = (buf[bi] & 0x3f) << 16 |
                         (buf[bi+1]) << 8 |
                         (buf[bi+2]);
    adaptation_field_extension->set_piecewise_rate(piecewise_rate);
    bi += 3;
  }
  if (seamless_splice_flag) {
    int splice_type = (buf[bi] & 0xf0) >> 4;
    adaptation_field_extension->set_splice_type(splice_type);
    int64_t dts_next_au = (((int64_t)(buf[bi+0] & 0x0e) << 30) |
        ((int64_t)(buf[bi+1]) << 22) |
        ((int64_t)(buf[bi+2] & 0xfe) << 15) |
        ((int64_t)(buf[bi+3]) << 7) |
        ((int64_t)(buf[bi+4] & 0xfe)));
    adaptation_field_extension->set_dts_next_au(dts_next_au);
    bi += 5;
  }
  // ignore reserved bytes
  return 1 + adaptation_field_extension_length;
}


int Mpeg2TsParser::DumpAdaptationFieldExtension(
    const AdaptationFieldExtension &adaptation_field_extension,
    uint8_t *buf, int len) {
  int bi = 0;

  buf[bi] = adaptation_field_extension.adaptation_field_extension_length();
  bi += 1;
  // ltw_flag
  BitSet(buf+bi, 0, 1, adaptation_field_extension.has_ltw_valid_flag());
  // piecewise_rate_flag
  BitSet(buf+bi, 1, 1, adaptation_field_extension.has_piecewise_rate());
  // seamless_splice_flag
  BitSet(buf+bi, 2, 1, adaptation_field_extension.has_splice_type());
  // reserved
  BitSet(buf+bi, 3, 5, 0x1f);
  bi += 1;
  if (adaptation_field_extension.has_ltw_valid_flag()) {
    // ltw_valid_flag
    BitSet(buf+bi, 0, 1, adaptation_field_extension.ltw_valid_flag());
    // ltw_offset
    BitSet(buf+bi, 1, 15, adaptation_field_extension.ltw_offset());
    bi += 2;
  }
  if (adaptation_field_extension.has_piecewise_rate()) {
    // reserved
    BitSet(buf+bi, 0, 2, 3);
    // piecewise_rate
    BitSet(buf+bi, 2, 22, adaptation_field_extension.piecewise_rate());
    bi += 3;
  }
  if (adaptation_field_extension.has_splice_type()) {
    // piecewise_rate
    BitSet(buf+bi, 0, 4, adaptation_field_extension.splice_type());
    // dts_next_au[32..30]
    int64_t dts_next_au = adaptation_field_extension.dts_next_au();
    BitSet(buf+bi, 4, 3, BitGet(dts_next_au, 30, 32));
    // marker
    BitSet(buf+bi, 7, 1, 1);
    bi += 1;
    // dts_next_au[29..15]
    BitSet(buf+bi, 0, 15, BitGet(dts_next_au, 15, 29));
    // marker
    BitSet(buf+bi, 7, 1, 1);
    bi += 2;
    // dts_next_au[14..0]
    BitSet(buf+bi, 0, 15, BitGet(dts_next_au, 0, 14));
    // marker
    BitSet(buf+bi, 7, 1, 1);
    bi += 2;
  }

  // add stuffing bytes
  while (bi < (1 +
      adaptation_field_extension.adaptation_field_extension_length())) {
    buf[bi++] = 0xff;
  }
  return bi;
}


int Mpeg2TsParser::ParsePCR(const uint8_t *buf, int len, PCR *pcr) {
  // +---+---+---+---+---+---+---+---+
  // |          base[32:25]          |
  // +---+---+---+---+---+---+---+---+
  // |          base[24:17]          |
  // +---+---+---+---+---+---+---+---+
  // |           base[16:9]          |
  // +---+---+---+---+---+---+---+---+
  // |           base[8:1]           |
  // +---+---+---+---+---+---+---+---+
  // |b.0| 1   1   1   1   1   1 |x.8|
  // +---+---+---+---+---+---+---+---+
  // |         extension[7:0]        |
  // +---+---+---+---+---+---+---+---+

  // ensure PCR can be parsed
  if (len < 6)
    return -1;

  // check that the padding bits are 0
  int padding = (buf[4] & 0x7e) >> 1;
  if (padding != 0x3f)
    return -1;
  // parse base/extension
  int64_t base = (((int64_t)(buf[0]) << 25) |
      ((int64_t)(buf[1]) << 17) |
      ((int64_t)(buf[2]) << 9) |
      ((int64_t)(buf[3]) << 1) |
      (((int64_t)(buf[4]) & 0x80) >> 7));
  int extension = ((buf[4] & 0x01) << 8) | buf[5];
  pcr->set_base(base);
  pcr->set_extension(extension);
  return 6;
}


int Mpeg2TsParser::DumpPCR(const PCR &pcr, uint8_t *buf, int len) {
  int bi = 0;
  int64_t base = pcr.base();
  int extension = pcr.extension();

  // base[32:25]
  BitSet(buf+bi, 0, 8, BitGet(base, 25, 32));
  bi += 1;
  // base[17:24]
  BitSet(buf+bi, 0, 8, BitGet(base, 17, 24));
  bi += 1;
  // base[9:16]
  BitSet(buf+bi, 0, 8, BitGet(base, 9, 16));
  bi += 1;
  // base[1:8]
  BitSet(buf+bi, 0, 8, BitGet(base, 1, 8));
  bi += 1;
  // base[0]
  BitSet(buf+bi, 0, 1, BitGet(base, 0, 0));
  // markers
  BitSet(buf+bi, 1, 6, 0x3f);
  // extension[8]
  BitSet(buf+bi, 7, 1, BitGet(extension, 8, 8));
  bi += 1;
  // extension[0:7]
  BitSet(buf+bi, 0, 8, BitGet(extension, 0, 7));
  bi += 1;
  return bi;
}


int Mpeg2TsParser::ParseESCR(const uint8_t *buf, int len, PCR *pcr) {
  // +---+---+---+---+---+---+---+---+
  // | 1 | 1 |base[32:30]| 1 |b29|b28|
  // +---+---+---+---+---+---+---+---+
  // |          base[27:20]          |
  // +---+---+---+---+---+---+---+---+
  // |     base[19:15]   | 1 |b14|b13|
  // +---+---+---+---+---+---+---+---+
  // |          base[12:5]           |
  // +---+---+---+---+---+---+---+---+
  // |      base[4:0]    | 1 |x.8|x.7|
  // +---+---+---+---+---+---+---+---+
  // |       extension[6:0]      | 1 |
  // +---+---+---+---+---+---+---+---+

  // ensure ESCR can be parsed
  if (len < 6)
    return -1;

  // parse base/extension
  int64_t base = (((int64_t)(buf[0] & 0x38) << 27) |
      (((int64_t)(buf[0] & 0x03) << 28) |
      ((int64_t)(buf[1]) << 20) |
      ((int64_t)(buf[2] & 0xf8) << 12) |
      ((int64_t)(buf[2] & 0x03) << 13) |
      ((int64_t)(buf[3]) << 5) |
      (((int64_t)(buf[4]) & 0xf8) >> 3)));
  int extension = (((int)(buf[4] & 0x03) << 8) |
      (((int)(buf[5]) & 0xfe) >> 1));
  pcr->set_base(base);
  pcr->set_extension(extension);
  return 6;
}


int Mpeg2TsParser::DumpESCR(const PCR &pcr, uint8_t *buf, int len) {
  int bi = 0;
  int64_t base = pcr.base();
  int extension = pcr.extension();

  // markers
  BitSet(buf+bi, 0, 2, 3);
  // base[32:30]
  BitSet(buf+bi, 2, 3, BitGet(base, 30, 32));
  // marker
  BitSet(buf+bi, 5, 1, 1);
  // base[29:28]
  BitSet(buf+bi, 6, 2, BitGet(base, 28, 29));
  bi += 1;
  // base[27:20]
  BitSet(buf+bi, 0, 8, BitGet(base, 20, 27));
  bi += 1;
  // base[19:15]
  BitSet(buf+bi, 0, 5, BitGet(base, 15, 19));
  // marker
  BitSet(buf+bi, 5, 1, 1);
  // base[14:13]
  BitSet(buf+bi, 6, 2, BitGet(base, 13, 14));
  bi += 1;
  // base[5:12]
  BitSet(buf+bi, 0, 8, BitGet(base, 5, 12));
  bi += 1;
  // base[0:4]
  BitSet(buf+bi, 0, 5, BitGet(base, 0, 4));
  // marker
  BitSet(buf+bi, 5, 1, 1);
  // extension[7:8]
  BitSet(buf+bi, 6, 2, BitGet(extension, 7, 8));
  bi += 1;
  // extension[0:6]
  BitSet(buf+bi, 0, 7, BitGet(extension, 0, 6));
  // marker
  BitSet(buf+bi, 7, 1, 1);
  bi += 1;
  return bi;
}


int Mpeg2TsParser::ParseESRate(const uint8_t *buf, int len, int *es_rate) {
  // +---+---+---+---+---+---+---+---+
  // | 1 |        es_rate[21:15]     |
  // +---+---+---+---+---+---+---+---+
  // |          es_rate[14:7]        |
  // +---+---+---+---+---+---+---+---+
  // |        es_rate[6:0]       | 1 |
  // +---+---+---+---+---+---+---+---+

  // ensure ESRate can be parsed
  if (len < 3)
    return -1;

  *es_rate = (((int)(buf[0] & 0x7f) << 15) |
      (((int)(buf[1])) << 7) |
      (((int)(buf[2]) & 0xfe) >> 1));
  return 3;
}


int Mpeg2TsParser::DumpESRate(const int es_rate, uint8_t *buf, int len) {
  int bi = 0;

  // marker
  BitSet(buf+bi, 0, 1, 1);
  // es_rate[15:21]
  BitSet(buf+bi, 1, 7, BitGet(es_rate, 15, 21));
  bi += 1;
  // es_rate[7:14]
  BitSet(buf+bi, 0, 8, BitGet(es_rate, 7, 14));
  bi += 1;
  // es_rate[0:6]
  BitSet(buf+bi, 0, 7, BitGet(es_rate, 0, 6));
  // marker
  BitSet(buf+bi, 7, 1, 1);
  bi += 1;
  return bi;
}


enum StreamIdType {
  STREAM_ID_OTHER = 0,
  STREAM_ID_PROGRAM_STREAM_MAP = 0xbc,
  STREAM_ID_PRIVATE_STREAM_1   = 0xbd,
  STREAM_ID_PADDING_STREAM     = 0xbe,
  STREAM_ID_PRIVATE_STREAM_2   = 0xbf,
  STREAM_ID_AUDIO_13818_BEGIN  = 0xc0,
  STREAM_ID_AUDIO_13818_END    = 0xdf,
  STREAM_ID_VIDEO_13818_BEGIN  = 0xe0,
  STREAM_ID_VIDEO_13818_END    = 0xef,
  STREAM_ID_ECM_STREAM         = 0xf0,
  STREAM_ID_EMM_STREAM         = 0xf1,
  STREAM_ID_DSMCC_STREAM       = 0xf2,
  STREAM_ID_13522_STREAM       = 0xf3,
  STREAM_ID_H222_A_STREAM      = 0xf4,
  STREAM_ID_H222_B_STREAM      = 0xf5,
  STREAM_ID_H222_C_STREAM      = 0xf6,
  STREAM_ID_H222_D_STREAM      = 0xf7,
  STREAM_ID_H222_E_STREAM      = 0xf8,
  STREAM_ID_ANCILLARY_STREAM   = 0xf9,
  STREAM_ID_PROGRAM_STREAM_DIRECTORY = 0xff,
};

PesPacket::StreamIdType Mpeg2TsParser::GetStreamIdType(int stream_id) {
  if (stream_id == STREAM_ID_PROGRAM_STREAM_MAP)
    return PesPacket::STREAM_ID_PROGRAM_STREAM_MAP;
  else if (stream_id == STREAM_ID_PRIVATE_STREAM_1)
    return PesPacket::STREAM_ID_PRIVATE_STREAM_1;
  else if (stream_id == STREAM_ID_PADDING_STREAM)
    return PesPacket::STREAM_ID_PADDING_STREAM;
  else if (stream_id == STREAM_ID_PRIVATE_STREAM_2)
    return PesPacket::STREAM_ID_PRIVATE_STREAM_2;
  else if ((stream_id >= STREAM_ID_AUDIO_13818_BEGIN) &&
      (stream_id <= STREAM_ID_AUDIO_13818_END))
    return PesPacket::STREAM_ID_AUDIO_13818;
  else if ((stream_id >= STREAM_ID_VIDEO_13818_BEGIN) &&
      (stream_id <= STREAM_ID_VIDEO_13818_END))
    return PesPacket::STREAM_ID_VIDEO_13818;
  else if (stream_id == STREAM_ID_ECM_STREAM)
    return PesPacket::STREAM_ID_ECM_STREAM;
  else if (stream_id == STREAM_ID_EMM_STREAM)
    return PesPacket::STREAM_ID_EMM_STREAM;
  else if (stream_id == STREAM_ID_DSMCC_STREAM)
    return PesPacket::STREAM_ID_DSMCC_STREAM;
  else if (stream_id == STREAM_ID_13522_STREAM)
    return PesPacket::STREAM_ID_13522_STREAM;
  else if (stream_id == STREAM_ID_H222_A_STREAM)
    return PesPacket::STREAM_ID_H222_A_STREAM;
  else if (stream_id == STREAM_ID_H222_B_STREAM)
    return PesPacket::STREAM_ID_H222_B_STREAM;
  else if (stream_id == STREAM_ID_H222_C_STREAM)
    return PesPacket::STREAM_ID_H222_C_STREAM;
  else if (stream_id == STREAM_ID_H222_D_STREAM)
    return PesPacket::STREAM_ID_H222_D_STREAM;
  else if (stream_id == STREAM_ID_H222_E_STREAM)
    return PesPacket::STREAM_ID_H222_E_STREAM;
  else if (stream_id == STREAM_ID_ANCILLARY_STREAM)
    return PesPacket::STREAM_ID_ANCILLARY_STREAM;
  else if (stream_id == STREAM_ID_PROGRAM_STREAM_DIRECTORY)
    return PesPacket::STREAM_ID_PROGRAM_STREAM_DIRECTORY;
  return PesPacket::STREAM_ID_OTHER;
}

int Mpeg2TsParser::ParsePTS(const uint8_t *buf, int len, int64_t *pts) {
  // +---+---+---+---+---+---+---+---+
  // | 0 | 0 | X | X | pts[32:30]| 1 |
  // +---+---+---+---+---+---+---+---+
  // |           pts[29:22]          |
  // +---+---+---+---+---+---+---+---+
  // |           pts[21:15]      | 1 |
  // +---+---+---+---+---+---+---+---+
  // |           pts[14:7]           |
  // +---+---+---+---+---+---+---+---+
  // |           pts[6:0]        | 1 |
  // +---+---+---+---+---+---+---+---+

  // ensure pts can be parsed
  if (len < 5)
    return -1;

  *pts = 0;
  // check the guard
  int guard = (buf[0] & 0xf0) >> 4;
  if (guard != 1 && guard != 2 && guard != 3)
    // invalid guard
    return -1;
  // check the markers
  if (((buf[0] & 0x01) != 0x01) ||
      ((buf[2] & 0x01) != 0x01) ||
      ((buf[4] & 0x01) != 0x01))
    // invalid markers
    return -1;
  *pts = ((((int64_t)(buf[0]) & 0x0e) << 29) |
      (((int64_t)(buf[1]) & 0xff) << 22) |
      (((int64_t)(buf[2]) & 0xfe) << 14) |
      (((int64_t)(buf[3]) & 0xff) << 7) |
      (((int64_t)(buf[4]) & 0xfe) >> 1));
  // printf("found pts: %li\n", *pts);
  return 5;
}


int Mpeg2TsParser::DumpPTS(const int64_t pts, int start,
    uint8_t *buf, int len) {
  int bi = 0;

  // start[0:3]
  BitSet(buf+bi, 0, 4, BitGet(start, 0, 3));
  // pts[32..30]
  BitSet(buf+bi, 4, 3, BitGet(pts, 30, 32));
  // marker
  BitSet(buf+bi, 7, 1, 1);
  bi += 1;
  // pts[29..15]
  BitSet(buf+bi, 0, 15, BitGet(pts, 15, 29));
  // marker
  BitSet(buf+bi+1, 7, 1, 1);
  bi += 2;
  // pts[0..14]
  BitSet(buf+bi, 0, 15, BitGet(pts, 0, 14));
  // marker
  BitSet(buf+bi+1, 7, 1, 1);
  bi += 2;
  return bi;
}

enum DsmTrickModeType {
  DSM_TRICK_MODE_FAST_FORWARD = 0,
  DSM_TRICK_MODE_SLOW_MOTION = 1,
  DSM_TRICK_MODE_FREEZE_FRAME = 2,
  DSM_TRICK_MODE_FAST_REVERSE = 3,
  DSM_TRICK_MODE_SLOW_REVERSE = 4,
};


int Mpeg2TsParser::ParseDsmTrickMode(const uint8_t *buf, int len,
    DsmTrickMode *dsm_trick_mode) {
  int bi = 0;
  // ensure flags can be parsed
  if (len < 1)
    return -1;

  int trick_mode_control = ((int)(buf[bi] & 0xe0) >> 5);
  dsm_trick_mode->set_trick_mode_control(trick_mode_control);
  if ((trick_mode_control == DSM_TRICK_MODE_FAST_FORWARD) ||
      (trick_mode_control == DSM_TRICK_MODE_FAST_REVERSE)) {
    int field_id = ((int)(buf[bi] & 0x18) >> 3);
    dsm_trick_mode->set_field_id(field_id);
    bool intra_slice_refresh = ((int)(buf[bi] & 0x04) >> 2);
    dsm_trick_mode->set_intra_slice_refresh(intra_slice_refresh);
    int frequency_truncation = (int)(buf[bi] & 0x03);
    dsm_trick_mode->set_frequency_truncation(frequency_truncation);

  } else if ((trick_mode_control == DSM_TRICK_MODE_SLOW_MOTION) ||
      (trick_mode_control == DSM_TRICK_MODE_SLOW_REVERSE)) {
    int rep_cntrl = (int)(buf[bi] & 0x1f);
    dsm_trick_mode->set_rep_cntrl(rep_cntrl);

  } else if (trick_mode_control == DSM_TRICK_MODE_FREEZE_FRAME) {
    int field_id = ((int)(buf[bi] & 0x18) >> 3);
    dsm_trick_mode->set_field_id(field_id);
  }
  bi += 1;
  return bi;
}


int Mpeg2TsParser::DumpDsmTrickMode(const DsmTrickMode &dsm_trick_mode,
    uint8_t *buf, int len) {
  int bi = 0;

  // trick_mode_control
  BitSet(buf+bi, 0, 3, dsm_trick_mode.trick_mode_control());

  int trick_mode_control = dsm_trick_mode.trick_mode_control();
  if ((trick_mode_control == DSM_TRICK_MODE_FAST_FORWARD) ||
      (trick_mode_control == DSM_TRICK_MODE_FAST_REVERSE)) {
    // field_id
    BitSet(buf+bi, 3, 2, dsm_trick_mode.field_id());
    // intra_slice_refresh
    BitSet(buf+bi, 5, 1, dsm_trick_mode.intra_slice_refresh());
    // frequency_truncation
    BitSet(buf+bi, 6, 2, dsm_trick_mode.frequency_truncation());

  } else if ((trick_mode_control == DSM_TRICK_MODE_SLOW_MOTION) ||
      (trick_mode_control == DSM_TRICK_MODE_SLOW_REVERSE)) {
    // rep_cntrl
    BitSet(buf+bi, 3, 5, dsm_trick_mode.rep_cntrl());

  } else if (trick_mode_control == DSM_TRICK_MODE_FREEZE_FRAME) {
    // field_id
    BitSet(buf+bi, 3, 2, dsm_trick_mode.field_id());
    // reserved
    BitSet(buf+bi, 5, 3, 0x07);
  } else {
    // reserved
    BitSet(buf+bi, 3, 5, 0x1f);
  }

  bi += 1;
  return bi;
}


int Mpeg2TsParser::ParsePesExtension(const uint8_t *buf, int len,
    PesExtension *pes_extension) {
  int bi = 0;
  // ensure flags can be parsed
  if (len < 1)
    return -1;

  int pes_private_data_flag = ((int)(buf[bi] & 0x80) >> 7);
  int pack_header_field_flag = ((int)(buf[bi] & 0x40) >> 6);
  int program_packet_sequence_counter_flag = ((int)(buf[bi] & 0x20) >> 5);
  int p_std_buffer_flag = ((int)(buf[bi] & 0x10) >> 4);
  int pes_extension_flag_2 = ((int)(buf[bi] & 0x01));
  bi += 1;
  if (pes_private_data_flag) {
    if ((len - bi) < 16)
      return -1;

    pes_extension->set_pes_private_data(buf+bi, 16);
    bi += 16;
  }
  if (pack_header_field_flag) {
    if ((len - bi) < 1)
      return -1;
    int pack_field_length = ((int)(buf[bi]));
    bi += 1;
    if ((len - bi) < pack_field_length)
      return -1;
    pes_extension->set_pack_header(buf+bi, pack_field_length);
    bi += pack_field_length;
  }
  if (program_packet_sequence_counter_flag) {
    if ((len - bi) < 2)
      return -1;
    int program_packet_sequence_counter = ((int)(buf[bi] & 0x7f) >> 1);
    pes_extension->set_program_packet_sequence_counter(
        program_packet_sequence_counter);
    bi += 1;
    int mpeg1_mpeg2_identifier = ((int)(buf[bi] & 0x40) >> 6);
    pes_extension->set_mpeg1_mpeg2_identifier(mpeg1_mpeg2_identifier);
    int original_stuff_length = ((int)(buf[bi] & 0x3f));
    pes_extension->set_original_stuff_length(original_stuff_length);
    bi += 1;
  }
  if (p_std_buffer_flag) {
    if ((len - bi) < 2)
      return -1;
    int p_std_buffer_scale = ((int)(buf[bi] & 0x20) >> 5);
    pes_extension->set_p_std_buffer_scale(p_std_buffer_scale);
    int p_std_buffer_size = ((int)(buf[bi] & 0x1f) << 4) |
        ((int)(buf[bi+1]));
    pes_extension->set_p_std_buffer_size(p_std_buffer_size);
    bi += 2;
  }
  if (pes_extension_flag_2) {
    if ((len - bi) < 1)
      return -1;
    int pes_extension_field_length = ((int)(buf[bi]));
    bi += 1;
    if ((len - bi) < pes_extension_field_length)
      return -1;
    pes_extension->set_pes_extension_field(buf+bi, pes_extension_field_length);
    bi += pes_extension_field_length;
  }
  return bi;
}


int Mpeg2TsParser::DumpPesExtension(const PesExtension &pes_extension,
    uint8_t *buf, int len) {
  int bi = 0;
  int res;

  // pes_private_data_flag
  BitSet(buf+bi, 0, 1, pes_extension.has_pes_private_data());
  // pack_header_field_flag
  BitSet(buf+bi, 1, 1, pes_extension.has_pack_header());
  // program_packet_sequence_counter_flag
  BitSet(buf+bi, 2, 1, pes_extension.has_program_packet_sequence_counter());
  // p_std_buffer_flag
  BitSet(buf+bi, 3, 1, pes_extension.has_p_std_buffer_scale());
  // reserved
  BitSet(buf+bi, 4, 2, 3);
  // pes_extension_flag_2
  BitSet(buf+bi, 3, 1, pes_extension.has_pes_extension_field());
  bi += 1;

  if (pes_extension.has_pes_private_data()) {
    res = pes_extension.pes_private_data().copy((char *)(buf + bi), 16);
    bi += res;
  }

  if (pes_extension.has_pack_header()) {
    buf[bi] = pes_extension.pack_header().length();
    bi += 1;
    res = pes_extension.pack_header().copy((char *)(buf + bi), len - bi);
    bi += res;
  }

  if (pes_extension.has_program_packet_sequence_counter()) {
    // marker
    BitSet(buf+bi, 0, 1, 1);
    // program_packet_sequence_counter
    BitSet(buf+bi, 1, 7, pes_extension.program_packet_sequence_counter());
    bi += 1;
    // marker
    BitSet(buf+bi, 0, 1, 1);
    // mpeg1_mpeg2_identifier
    BitSet(buf+bi, 1, 1, pes_extension.mpeg1_mpeg2_identifier());
    // original_stuff_length
    BitSet(buf+bi, 2, 6, pes_extension.original_stuff_length());
    bi += 1;
  }

  if (pes_extension.has_p_std_buffer_scale()) {
    // '01'
    BitSet(buf+bi, 0, 2, 1);
    // p_std_buffer_scale
    BitSet(buf+bi, 2, 1, pes_extension.p_std_buffer_scale());
    // p_std_buffer_size
    BitSet(buf+bi, 3, 13, pes_extension.p_std_buffer_size());
    bi += 2;
  }

  if (pes_extension.has_pes_extension_field()) {
    // marker
    BitSet(buf+bi, 0, 1, 1);
    // pes_extension_field_length
    BitSet(buf+bi, 1, 7, pes_extension.pes_extension_field().length());
    bi += 1;
    res = pes_extension.pes_extension_field().copy((char *)(buf + bi),
        len - bi);
    bi += res;
  }

  return bi;
}


int Mpeg2TsParser::ParsePesPacket(const uint8_t *buf, int len,
    PesPacket *pes_packet) {
  int bi = 0;
  int res;
  if ((len - bi) < 6)
    return -1;
  // skip packet start code prefix
  bi += 3;
  int stream_id = buf[bi];
  pes_packet->set_stream_id(stream_id);
  PesPacket::StreamIdType stream_id_type = GetStreamIdType(stream_id);
  pes_packet->set_stream_id_type(stream_id_type);
  bi += 1;
  int pes_packet_length = ((int)(buf[bi]) << 8) | ((int)(buf[bi+1]));
  pes_packet->set_pes_packet_length(pes_packet_length);
  bi += 2;
  if (stream_id_type != PesPacket::STREAM_ID_PROGRAM_STREAM_MAP &&
      stream_id_type != PesPacket::STREAM_ID_PADDING_STREAM &&
      stream_id_type != PesPacket::STREAM_ID_PRIVATE_STREAM_2 &&
      stream_id_type != PesPacket::STREAM_ID_ECM_STREAM &&
      stream_id_type != PesPacket::STREAM_ID_EMM_STREAM &&
      stream_id_type != PesPacket::STREAM_ID_DSMCC_STREAM &&
      stream_id_type != PesPacket::STREAM_ID_H222_E_STREAM &&
      stream_id_type != PesPacket::STREAM_ID_PROGRAM_STREAM_DIRECTORY) {
    if ((len - bi) < 3)
      return -1;
    int pes_scrambling_control = ((int)(buf[bi] & 0x30) >> 4);
    pes_packet->set_pes_scrambling_control(pes_scrambling_control);
    int pes_priority = ((int)(buf[bi] & 0x08) >> 3);
    pes_packet->set_pes_priority(pes_priority);
    int data_alignment_indicator = ((int)(buf[bi] & 0x04) >> 2);
    pes_packet->set_data_alignment_indicator(data_alignment_indicator);
    int copyright = ((int)(buf[bi] & 0x02) >> 1);
    pes_packet->set_copyright(copyright);
    int original_or_copy = ((int)(buf[bi] & 0x01));
    pes_packet->set_original_or_copy(original_or_copy);
    bi += 1;
    int pts_flag = ((int)(buf[bi] & 0x80) >> 7);
    int dts_flag = ((int)(buf[bi] & 0x40) >> 6);
    int escr_flag = ((int)(buf[bi] & 0x20) >> 5);
    int es_rate_flag = ((int)(buf[bi] & 0x10) >> 4);
    int dsm_trick_mode_flag = ((int)(buf[bi] & 0x08) >> 3);
    int additional_copy_info_flag = ((int)(buf[bi] & 0x04) >> 2);
    int pes_crc_flag = ((int)(buf[bi] & 0x02) >> 1);
    int pes_extension_flag = ((int)(buf[bi] & 0x01));
    bi += 1;
    int pes_header_data_length = (int)(buf[bi]);
    pes_packet->set_pes_header_data_length(pes_header_data_length);
    bi += 1;
    int fixed_bi = bi;
    if (pts_flag) {
      int64_t pts = 0;
      res = ParsePTS(buf + bi, len - bi, &pts);
      if (res < 0)
        return -1;
      pes_packet->set_pts(pts);
      bi += res;
    }
    if (dts_flag) {
      int64_t dts = 0;
      res = ParsePTS(buf + bi, len - bi, &dts);
      if (res < 0)
        return -1;
      pes_packet->set_dts(dts);
      bi += res;
    }
    if (escr_flag) {
      res = ParseESCR(buf + bi, len - bi, pes_packet->mutable_escr());
      if (res < 0)
        return -1;
      bi += res;
    }
    if (es_rate_flag) {
      int es_rate = 0;
      res = ParseESRate(buf + bi, len - bi, &es_rate);
      if (res < 0)
        return -1;
      pes_packet->set_es_rate(es_rate);
      bi += res;
    }
    if (dsm_trick_mode_flag) {
      res = ParseDsmTrickMode(buf + bi, len - bi,
          pes_packet->mutable_dsm_trick_mode());
      if (res < 0)
        return -1;
      bi += res;
    }
    if (additional_copy_info_flag) {
      int additional_copy_info = buf[bi] & 0x7f;
      pes_packet->set_additional_copy_info(additional_copy_info);
      bi += 1;
    }
    if (pes_crc_flag) {
      int previous_pes_packet_crc = (buf[bi] << 8) |
          buf[bi + 1];
      pes_packet->set_previous_pes_packet_crc(previous_pes_packet_crc);
      bi += 2;
    }
    if (pes_extension_flag) {
      res = ParsePesExtension(buf + bi, len - bi,
          pes_packet->mutable_pes_extension());
      if (res < 0)
        return -1;
      bi += res;
    }
    // skip stuffing bytes
    if (bi < (fixed_bi + pes_header_data_length))
      bi = fixed_bi + pes_header_data_length;
    // remaining is data bytes

  } else if (stream_id_type == PesPacket::STREAM_ID_PROGRAM_STREAM_MAP ||
      stream_id_type == PesPacket::STREAM_ID_PRIVATE_STREAM_2 ||
      stream_id_type == PesPacket::STREAM_ID_ECM_STREAM ||
      stream_id_type == PesPacket::STREAM_ID_EMM_STREAM ||
      stream_id_type == PesPacket::STREAM_ID_DSMCC_STREAM ||
      stream_id_type == PesPacket::STREAM_ID_H222_E_STREAM ||
      stream_id_type == PesPacket::STREAM_ID_PROGRAM_STREAM_DIRECTORY) {
    ;

  } else if (stream_id_type != PesPacket::STREAM_ID_PADDING_STREAM) {
    // remaining is padding
    pes_packet->set_padding_byte(buf + bi, len - bi);
    bi = len;
  }
  return bi;
}


int Mpeg2TsParser::DumpPesPacket(const PesPacket &pes_packet,
    uint8_t *buf, int len) {
  int bi = 0;
  int res;

  // packet_start_code_prefix
  buf[bi] = 0x00;
  bi += 1;
  buf[bi] = 0x00;
  bi += 1;
  buf[bi] = 0x01;
  bi += 1;
  // stream_id
  buf[bi] = pes_packet.stream_id_type();
  bi += 1;
  // pes_packet_length
  BitSet(buf+bi, 0, 16, pes_packet.pes_packet_length());
  bi += 2;
  //
  auto stream_id_type = pes_packet.stream_id_type();
  if (stream_id_type != PesPacket::STREAM_ID_PROGRAM_STREAM_MAP &&
      stream_id_type != PesPacket::STREAM_ID_PADDING_STREAM &&
      stream_id_type != PesPacket::STREAM_ID_PRIVATE_STREAM_2 &&
      stream_id_type != PesPacket::STREAM_ID_ECM_STREAM &&
      stream_id_type != PesPacket::STREAM_ID_EMM_STREAM &&
      stream_id_type != PesPacket::STREAM_ID_DSMCC_STREAM &&
      stream_id_type != PesPacket::STREAM_ID_H222_E_STREAM &&
      stream_id_type != PesPacket::STREAM_ID_PROGRAM_STREAM_DIRECTORY) {
    // '10'
    BitSet(buf+bi, 0, 2, 2);
    // pes_scrambling_control
    BitSet(buf+bi, 2, 2, pes_packet.pes_scrambling_control());
    // pes_priority
    BitSet(buf+bi, 4, 1, pes_packet.pes_priority());
    // data_alignment_indicator
    BitSet(buf+bi, 5, 1, pes_packet.data_alignment_indicator());
    // copyright
    BitSet(buf+bi, 6, 1, pes_packet.copyright());
    // original_or_copy
    BitSet(buf+bi, 7, 1, pes_packet.original_or_copy());
    bi += 1;
    // pts_flag
    BitSet(buf+bi, 0, 1, pes_packet.has_pts());
    // dts_flag
    BitSet(buf+bi, 1, 1, pes_packet.has_dts());
    // escr_flag
    BitSet(buf+bi, 2, 1, pes_packet.has_escr());
    // es_rate_flag
    BitSet(buf+bi, 3, 1, pes_packet.has_es_rate());
    // dsm_trick_mode_flag
    BitSet(buf+bi, 4, 1, pes_packet.has_dsm_trick_mode());
    // additional_copy_info_flag
    BitSet(buf+bi, 5, 1, pes_packet.has_additional_copy_info());
    // previous_pes_packet_crc
    BitSet(buf+bi, 6, 1, pes_packet.has_previous_pes_packet_crc());
    // pes_extension_flag
    BitSet(buf+bi, 7, 1, pes_packet.has_pes_extension());
    bi += 1;
    // pes_header_data_length
    buf[bi] = pes_packet.pes_header_data_length();
    bi += 1;
    int fixed_bi = bi;
    // pts
    if (pes_packet.has_pts()) {
      int start = 0x02;
      if (pes_packet.has_dts())
        start = 0x03;
      res = DumpPTS(pes_packet.pts(), start, buf + bi, len - bi);
      if (res < 0)
        return -1;
      bi += res;
    }
    // dts
    if (pes_packet.has_dts()) {
      int start = 0x01;
      res = DumpPTS(pes_packet.dts(), start, buf + bi, len - bi);
      if (res < 0)
        return -1;
      bi += res;
    }
    // escr
    if (pes_packet.has_escr()) {
      res = DumpESCR(pes_packet.escr(), buf + bi, len - bi);
      if (res < 0)
        return -1;
      bi += res;
    }
    // es_rate
    if (pes_packet.has_es_rate()) {
      res = DumpESRate(pes_packet.es_rate(), buf + bi, len - bi);
      if (res < 0)
        return -1;
      bi += res;
    }
    // dsm_trick_mode
    if (pes_packet.has_dsm_trick_mode()) {
      // pes_extension_flag
      res = DumpDsmTrickMode(pes_packet.dsm_trick_mode(), buf + bi, len - bi);
      if (res < 0)
        return -1;
      bi += res;
    }
    // additional_copy_info_flag
    if (pes_packet.has_additional_copy_info()) {
      // marker
      BitSet(buf+bi, 0, 1, 1);
      BitSet(buf+bi, 1, 7, pes_packet.additional_copy_info());
      bi += 1;
    }
    // previous_pes_packet_crc
    if (pes_packet.has_previous_pes_packet_crc()) {
      // previous_pes_packet_crc
      BitSet(buf+bi, 0, 16, pes_packet.previous_pes_packet_crc());
      bi += 2;
    }
    if (pes_packet.has_pes_extension()) {
      // pes_extension_flag
      res = DumpPesExtension(pes_packet.pes_extension(), buf + bi, len - bi);
      if (res < 0)
        return -1;
      bi += res;
    }
    // stuffing bytes
    while (bi < (fixed_bi + pes_packet.pes_header_data_length())) {
      buf[bi++] = 0xff;
    }

  } else if (stream_id_type == PesPacket::STREAM_ID_PROGRAM_STREAM_MAP ||
      stream_id_type == PesPacket::STREAM_ID_PRIVATE_STREAM_2 ||
      stream_id_type == PesPacket::STREAM_ID_ECM_STREAM ||
      stream_id_type == PesPacket::STREAM_ID_EMM_STREAM ||
      stream_id_type == PesPacket::STREAM_ID_DSMCC_STREAM ||
      stream_id_type == PesPacket::STREAM_ID_H222_E_STREAM ||
      stream_id_type == PesPacket::STREAM_ID_PROGRAM_STREAM_DIRECTORY) {
    // just data bytes
    ;

  } else if (stream_id_type != PesPacket::STREAM_ID_PADDING_STREAM) {
    // padding bytes
    res = pes_packet.padding_byte().copy((char *)(buf + bi), len - bi);
    bi += res;
  }
  return bi;
}


int Mpeg2TsParser::ParsePsiPacket(const uint8_t *buf, int len,
    PsiPacket *psi_packet) {
  int bi = 0;
  int res;
  if ((len - bi) < 1)
    return -1;
  int pointer_field_length = buf[bi];
  bi += 1;
  if ((len - bi) < pointer_field_length)
    return -1;
  psi_packet->set_pointer_field(buf + bi, pointer_field_length);
  bi += pointer_field_length;
  // parse the PSI sections
  while (bi < len) {
    // identify the following section
    int table_id = buf[bi];
    res = -1;
    if (table_id == MPEG_TS_TABLE_ID_PROGRAM_ASSOCIATION_SECTION) {
      res = ParseProgramAssociationSection(buf + bi, len - bi,
          psi_packet->add_program_association_section());
      if (res < 0) {
        psi_packet->clear_program_association_section();
      }
    } else if (table_id == MPEG_TS_TABLE_ID_TS_PROGRAM_MAP_SECTION) {
      res = ParseProgramMapSection(buf + bi, len - bi,
          psi_packet->add_program_map_section());
      if (res < 0) {
        psi_packet->clear_program_map_section();
      }
    } else if (table_id == MPEG_TS_TABLE_ID_FORBIDDEN) {
      // remaining bytes are data bytes
      break;
    }
    if (res < 0) {
      // unsupported PSI Section
      res = ParseOtherPsiSection(buf + bi, len - bi,
          psi_packet->add_other_psi_section());
    }
    if (res < 0) {
      return -1;
    }
    bi += res;
  }

  return bi;
}


int Mpeg2TsParser::DumpPsiPacket(const PsiPacket &psi_packet,
    uint8_t *buf, int len) {
  int bi = 0;
  int res;
  if (!psi_packet.has_pointer_field())
    return -1;
  if ((size_t) len < (psi_packet.pointer_field().length() + 1))
    return -1;
  buf[bi] = psi_packet.pointer_field().length();
  bi += 1;
  if (psi_packet.pointer_field().length() > (unsigned int)len)
    // not enough space for the pointer field
    return -1;
  res = psi_packet.pointer_field().copy((char *)(buf + bi), len - bi);
  bi += res;

  // dump PAT sections
  for (int i = 0; i < psi_packet.program_association_section_size(); ++i) {
    res = DumpProgramAssociationSection(
        psi_packet.program_association_section(i), buf + bi, len - bi);
    if (res < 0)
      return -1;
    bi += res;
  }

  // dump PAT sections
  for (int i = 0; i < psi_packet.program_map_section_size(); ++i) {
    res = DumpProgramMapSection(
        psi_packet.program_map_section(i), buf + bi, len - bi);
    if (res < 0)
      return -1;
    bi += res;
  }

  // dump unsupported PSI sections
  for (int i = 0; i < psi_packet.other_psi_section_size(); ++i) {
    res = DumpOtherPsiSection(
        psi_packet.other_psi_section(i), buf + bi, len - bi);
    if (res < 0)
      return -1;
    bi += res;
  }

  return bi;
}


int Mpeg2TsParser::ParseProgramAssociationSection(const uint8_t *buf, int len,
    ProgramAssociationSection *program_association_section) {
  int bi = 0;
  if ((len - bi) < 8)
    return -1;
  int table_id = buf[bi];
  program_association_section->set_table_id(table_id);
  bi += 1;
  // check next byte
  int section_syntax_indicator = (buf[bi] & 0x80) >> 7;
  if (section_syntax_indicator != 1)
    return -1;
  int zero = (buf[bi] & 0x40) >> 6;
  if (zero != 0)
    return -1;
  int reserved = (buf[bi] & 0x30) >> 4;
  if (reserved != 3)
    return -1;
  // "...first two bits of [section_length] shall be '00'. "
  int first_two_bits_of_section_length = (buf[bi] & 0x0c) >> 2;
  if (first_two_bits_of_section_length != 0)
    return -1;
  int section_length = ((buf[bi] & 0x0f) << 8) | buf[bi + 1];
  program_association_section->set_section_length(section_length);
  bi += 2;
  section_length += bi;
  if (len < section_length)
    return -1;
  int transport_stream_id = (buf[bi] << 8) | buf[bi + 1];
  program_association_section->set_transport_stream_id(transport_stream_id);
  bi += 2;
  int version_number = ((buf[bi] & 0x3e) >> 1);
  program_association_section->set_version_number(version_number);
  int current_next_indicator = (buf[bi] & 0x01);
  program_association_section->set_current_next_indicator(
      current_next_indicator);
  bi += 1;
  int section_number = buf[bi];
  program_association_section->set_section_number(section_number);
  bi += 1;
  int last_section_number = buf[bi];
  program_association_section->set_last_section_number(last_section_number);
  bi += 1;
  while (bi < (section_length - 4)) {
    int res = ParseProgramInformation(buf + bi, len - bi,
        program_association_section->add_program_information());
    if (res < 0) {
      return -1;
    }
    bi += res;
  }
  int crc_32 = (((int32_t)(buf[bi+0]) << 24) |
        ((int32_t)(buf[bi+1]) << 16) |
        ((int32_t)(buf[bi+2]) << 8) |
        ((int32_t)(buf[bi+3])));
  program_association_section->set_crc_32(crc_32);
  bi += 4;
  return bi;
}


int Mpeg2TsParser::DumpProgramAssociationSection(
    const ProgramAssociationSection &program_association_section,
    uint8_t *buf, int len) {
  int bi = 0;
  int res;

  if (len < 1)
    return -1;
  BitSet(buf+bi, 0, 8, program_association_section.table_id());
  bi += 1;

  // section_syntax_indicator
  BitSet(buf+bi, 0, 1, 1);
  // '0'
  BitSet(buf+bi, 1, 1, 0);
  // reserved
  BitSet(buf+bi, 2, 2, 3);
  // section_length
  BitSet(buf+bi, 4, 12, program_association_section.section_length());
  bi += 2;
  // transport_stream_id
  BitSet(buf+bi, 0, 16, program_association_section.transport_stream_id());
  bi += 2;
  // reserved
  BitSet(buf+bi, 0, 2, 3);
  // version_number
  BitSet(buf+bi, 2, 5, program_association_section.version_number());
  // current_next_indicator
  BitSet(buf+bi, 7, 1, program_association_section.current_next_indicator());
  bi += 1;
  // section_number
  BitSet(buf+bi, 0, 8, program_association_section.section_number());
  bi += 1;
  // last_section_number
  BitSet(buf+bi, 0, 8, program_association_section.last_section_number());
  bi += 1;
  // program information
  for (int i = 0; i < program_association_section.program_information_size();
      ++i) {
    res = DumpProgramInformation(
        program_association_section.program_information(i), buf + bi, len - bi);
    if (res < 0)
      return -1;
    bi += res;
  }
  // crc_32
  BitSet(buf+bi, 0, 32, program_association_section.crc_32());
  bi += 4;

  return bi;
}


int Mpeg2TsParser::ParseProgramInformation(const uint8_t *buf, int len,
    ProgramInformation *program_information) {
  int bi = 0;
  if ((len - bi) < 4)
    return -1;
  int program_number = (buf[bi] << 8) | buf[bi + 1];
  program_information->set_program_number(program_number);
  bi += 2;
  int pid = ((buf[bi] & 0x1f) << 8) | buf[bi + 1];
  if (program_number == 0)
    program_information->set_network_pid(pid);
  else
    program_information->set_program_map_pid(pid);
  bi += 2;
  return bi;
}


int Mpeg2TsParser::DumpProgramInformation(
    const ProgramInformation &program_information,
    uint8_t *buf, int len) {
  int bi = 0;

  BitSet(buf+bi, 0, 16, program_information.program_number());
  bi += 2;
  // reserved
  BitSet(buf+bi, 0, 3, 7);
  if (program_information.program_number() == 0)
    BitSet(buf+bi, 3, 13, program_information.network_pid());
  else
    BitSet(buf+bi, 3, 13, program_information.program_map_pid());
  bi += 2;
  return bi;
}


int Mpeg2TsParser::ParseProgramMapSection(const uint8_t *buf, int len,
    ProgramMapSection *program_map_section) {
  int bi = 0;
  int res;
  if ((len - bi) < 8)
    return -1;
  int table_id = buf[bi];
  program_map_section->set_table_id(table_id);
  bi += 1;
  // check next byte
  int section_syntax_indicator = (buf[bi] & 0x80) >> 7;
  if (section_syntax_indicator != 1)
    return -1;
  int zero = (buf[bi] & 0x40) >> 6;
  if (zero != 0)
    return -1;
  int reserved = (buf[bi] & 0x30) >> 4;
  if (reserved != 3)
    return -1;
  // "...first two bits of [section_length] shall be '00'. "
  int first_two_bits_of_section_length = (buf[bi] & 0x0c) >> 2;
  if (first_two_bits_of_section_length != 0)
    return -1;
  int section_length = ((buf[bi] & 0x0f) << 8) | buf[bi + 1];
  program_map_section->set_section_length(section_length);
  bi += 2;
  section_length += bi;
  if (len < section_length)
    return -1;
  int program_number = (buf[bi] << 8) | buf[bi + 1];
  program_map_section->set_program_number(program_number);
  bi += 2;
  int version_number = ((buf[bi] & 0x3e) >> 1);
  program_map_section->set_version_number(version_number);
  int current_next_indicator = (buf[bi] & 0x01);
  program_map_section->set_current_next_indicator(
      current_next_indicator);
  bi += 1;
  int section_number = buf[bi];
  program_map_section->set_section_number(section_number);
  bi += 1;
  int last_section_number = buf[bi];
  program_map_section->set_last_section_number(last_section_number);
  bi += 1;
  int pcr_pid = ((buf[bi] & 0x1f) << 8) | buf[bi + 1];
  program_map_section->set_pcr_pid(pcr_pid);
  bi += 2;
  int program_info_length = ((buf[bi] & 0x0f) << 8) | buf[bi + 1];
  program_map_section->set_program_info_length(program_info_length);
  bi += 2;
  // parse descriptors
  int fixed_bi = bi;
  while (bi < (fixed_bi + program_info_length)) {
    res = ParseDescriptor(buf + bi, program_info_length - (bi - fixed_bi),
        program_map_section->add_mpegts_descriptor());
    if (res < 0) {
      return -1;
    }
    bi += res;
  }
  // parse stream descriptors
  while (bi < (section_length - 4)) {
    res = ParseStreamDescription(buf + bi, section_length - 4 - bi,
        program_map_section->add_stream_description());
    if (res < 0) {
      return -1;
    }
    bi += res;
  }
  int crc_32 = (((int32_t)(buf[bi+0]) << 24) |
        ((int32_t)(buf[bi+1]) << 16) |
        ((int32_t)(buf[bi+2]) << 8) |
        ((int32_t)(buf[bi+3])));
  program_map_section->set_crc_32(crc_32);
  bi += 4;
  return bi;
}


int Mpeg2TsParser::DumpProgramMapSection(
    const ProgramMapSection &program_map_section,
    uint8_t *buf, int len) {
  int bi = 0;
  int res;

  if (len < 1)
    return -1;
  BitSet(buf+bi, 0, 8, program_map_section.table_id());
  bi += 1;

  // section_syntax_indicator
  BitSet(buf+bi, 0, 1, 1);
  // '0'
  BitSet(buf+bi, 1, 1, 0);
  // reserved
  BitSet(buf+bi, 2, 2, 3);
  // section_length
  BitSet(buf+bi, 4, 12, program_map_section.section_length());
  bi += 2;
  // program_number
  BitSet(buf+bi, 0, 16, program_map_section.program_number());
  bi += 2;
  // reserved
  BitSet(buf+bi, 0, 2, 3);
  // version_number
  BitSet(buf+bi, 2, 5, program_map_section.version_number());
  // current_next_indicator
  BitSet(buf+bi, 7, 1, program_map_section.current_next_indicator());
  bi += 1;
  // section_number
  BitSet(buf+bi, 0, 8, program_map_section.section_number());
  bi += 1;
  // last_section_number
  BitSet(buf+bi, 0, 8, program_map_section.last_section_number());
  bi += 1;
  // reserved
  BitSet(buf+bi, 0, 3, 7);
  // pcr_pid
  BitSet(buf+bi, 3, 13, program_map_section.pcr_pid());
  bi += 2;
  // reserved
  BitSet(buf+bi, 0, 4, 0x0f);
  // program_info_length
  BitSet(buf+bi, 4, 12, program_map_section.program_info_length());
  bi += 2;

  // dump descriptors
  for (int i = 0; i < program_map_section.mpegts_descriptor_size(); ++i) {
    res = DumpDescriptor(program_map_section.mpegts_descriptor(i),
        buf + bi, len - bi);
    if (res < 0) {
      return -1;
    }
    bi += res;
  }

  // dump stream descriptors
  for (int i = 0; i < program_map_section.stream_description_size(); ++i) {
    res = DumpStreamDescription(program_map_section.stream_description(i),
        buf + bi, len - bi);
    if (res < 0) {
      return -1;
    }
    bi += res;
  }

  // crc_32
  BitSet(buf+bi, 0, 32, program_map_section.crc_32());
  bi += 4;
  return bi;
}


int Mpeg2TsParser::ParseStreamDescription(const uint8_t *buf, int len,
    StreamDescription *stream_description) {
  int bi = 0;
  int res;
  if ((len - bi) < 5)
    return -1;
  int stream_type = buf[bi];
  stream_description->set_stream_type(stream_type);
  bi += 1;
  int elementary_pid = ((buf[bi] & 0x1f) << 8) | buf[bi + 1];
  stream_description->set_elementary_pid(elementary_pid);
  bi += 2;
  int es_info_length = ((buf[bi] & 0x0f) << 8) | buf[bi + 1];
  // ensure the first 2 out of the 12 bits are 0
  if ((es_info_length & 0xc00) != 0)
    return -1;
  stream_description->set_es_info_length(es_info_length);
  bi += 2;
  // parse descriptors
  int fixed_bi = bi;
  while (bi < (fixed_bi + es_info_length)) {
    res = ParseDescriptor(buf + bi, es_info_length - (bi - fixed_bi),
        stream_description->add_mpegts_descriptor());
    if (res < 0) {
      return -1;
    }
    bi += res;
  }
  return bi;
}


int Mpeg2TsParser::DumpStreamDescription(
    const StreamDescription &stream_description,
    uint8_t *buf, int len) {
  int bi = 0;
  int res;

  // stream_type
  buf[bi] = stream_description.stream_type();
  bi += 1;
  // reserved
  BitSet(buf+bi, 0, 3, 7);
  // elementary_pid
  BitSet(buf+bi, 3, 13, stream_description.elementary_pid());
  bi += 2;
  // reserved
  BitSet(buf+bi, 0, 4, 0xf);
  // es_info_length
  BitSet(buf+bi, 4, 12, stream_description.es_info_length());
  bi += 2;

  // descriptors
  for (int i = 0; i < stream_description.mpegts_descriptor_size(); ++i) {
    res = DumpDescriptor(stream_description.mpegts_descriptor(i),
        buf + bi, len - bi);
    if (res < 0) {
      return -1;
    }
    bi += res;
  }

  return bi;
}


int Mpeg2TsParser::ParseOtherPsiSection(const uint8_t *buf, int len,
    OtherPsiSection *other_psi_section) {
  int bi = 0;
  if ((len - bi) < 8)
    return -1;
  int table_id = buf[bi];
  other_psi_section->set_table_id(table_id);
  bi += 1;
  other_psi_section->set_remaining(buf + bi, len - bi);
  bi = len;
  return bi;
}


int Mpeg2TsParser::DumpOtherPsiSection(
    const OtherPsiSection &other_psi_section,
    uint8_t *buf, int len) {
  int bi = 0;
  int res;

  if (!other_psi_section.has_table_id())
    return -1;
  if (len < 1)
    return -1;
  buf[bi] = other_psi_section.table_id();
  bi += 1;
  if (other_psi_section.remaining().length() > (unsigned int)len)
    // not enough space for the remaining packet
    return -1;
  res = other_psi_section.remaining().copy((char *)(buf + bi), len - bi);
  bi += res;
  return bi;
}


int Mpeg2TsParser::ParseDescriptor(const uint8_t *buf, int len,
    Descriptor *mpegts_descriptor) {
  int bi = 0;
  if ((len - bi) < 2)
    return -1;
  int tag = buf[bi];
  mpegts_descriptor->set_tag(tag);
  bi += 1;
  int length = buf[bi];
  mpegts_descriptor->set_length(length);
  bi += 1;
  if (len < (bi + length))
    return -1;
  mpegts_descriptor->set_data(buf + bi, length);
  bi += length;
  return bi;
}


int Mpeg2TsParser::DumpDescriptor(const Descriptor &mpegts_descriptor,
    uint8_t *buf, int len) {
  int bi = 0;
  int res;

  buf[bi] = mpegts_descriptor.tag();
  bi += 1;
  buf[bi] = mpegts_descriptor.length();
  bi += 1;
  res = mpegts_descriptor.data().copy((char *)(buf + bi), len - bi);
  bi += res;
  return bi;
}
