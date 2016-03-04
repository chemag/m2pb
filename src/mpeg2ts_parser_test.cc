// Copyright Google Inc. Apache 2.0.

#include <stdio.h>  // for remove
#include <string.h>  // for memset
#include <unistd.h>  // for usleep

#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include "gmock_utils.h"
#include "mpeg2ts.pb.h"
#include "mpeg2ts_parser.h"

class TestableMpeg2TsParser : public Mpeg2TsParser {
 public:
  TestableMpeg2TsParser() : Mpeg2TsParser(true) {}
};

class Mpeg2TsParserTest : public ::testing::Test {
 protected:
  void SetUp() override {
  }

  void TearDown() override {
  }

  TestableMpeg2TsParser mpeg2ts_parser_;
};

// TODO(chema): add some tests. Really.

TEST_F(Mpeg2TsParserTest, SimpleTest) {
  EXPECT_EQ(0, 0);
}

static const char kMpeg2TsPacketWithPacketPayload[] = R"EOF(
packet: 0
byte: 0
parsed {
  header {
    transport_error_indicator: false
    payload_unit_start_indicator: true
    transport_priority: true
    pid: 290
    transport_scrambling_control: 2
    adaptation_field_exists: false
    payload_exists: true
    continuity_counter: 0
  }
  psi_packet {
    pointer_field: "E\"\317\313\331\001.\361n._I\253\376\227\202\310\344\262F\312\307_\342\032\202"
    other_psi_section {
      table_id: 108
      remaining: "\246\017\317\3329\022\353Q\370\262TJ\316K\024Mn}\027\207\271\202\262X\376\021\214\344\311\035E\334\204\262-n\025&\036\362;M\007wB\335B\252\035\270\245T$i,\035\005K\257\025G\275w\0012D\243\035\201\t9\001\224\306\372\013\371\036\014\002\016-*\266\240\021I9Mv\200\327\371\335.\322\016\t\210\355\311\267\231\244\326\244>\347\026\363W\251MK\3371\362|\333\263Kv\260\207\307\t\277RY\262\264\357\217\2609\272\317EC\320\353\003\211tw`f\033\205\2173\215\327\357\353c"
    }
  }
}
)EOF";

TEST_F(Mpeg2TsParserTest, ProtobufPackets) {
  struct protobuf_start_test {
    int line;
    const char *pb;
  } test_arr[] = {
    {__LINE__, kMpeg2TsPacketWithPacketPayload},
  };

  uint8_t buf[MPEG_TS_PACKET_SIZE];

  for (const auto &test_item : test_arr) {
    // parse pb packet
    Mpeg2Ts mpeg2ts;
    EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
        test_item.pb, &mpeg2ts))
        << "line " << test_item.line;
    // dump it to binary
    EXPECT_EQ(MPEG_TS_PACKET_SIZE,
        mpeg2ts_parser_.DumpPacket(mpeg2ts, buf, MPEG_TS_PACKET_SIZE))
        << "line " << test_item.line;
    // parse it back to pb
    Mpeg2Ts mpeg2ts_from_binary;
    mpeg2ts_parser_.ParsePacket(0, 0, buf, MPEG_TS_PACKET_SIZE,
        &mpeg2ts_from_binary);
    EXPECT_THAT(mpeg2ts, EqualsProto(mpeg2ts_from_binary))
        << "line " << test_item.line;
  }
}

const uint8_t mpts_pat_header[] = {
  0x47, 0x40, 0x00, 0x11, 0x00, 0x00, 0xb0, 0x29, 0x02, 0xa4, 0xf3, 0x00,
  0x00, 0x00, 0x00, 0xef, 0xfe, 0x00, 0x02, 0xe0, 0x29, 0x00, 0x03, 0xe0,
  0x69, 0x00, 0x97, 0xe0, 0x40, 0x00, 0x04, 0xe0, 0xa9, 0x00, 0x05, 0xe0,
  0xc9, 0x00, 0x06, 0xe0, 0xe9, 0x00, 0x07, 0xe1, 0x29, 0xb9, 0x72, 0x43,
  0xb8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

TEST_F(Mpeg2TsParserTest, BinaryPackets) {
  struct binary_start_test {
    int line;
    const uint8_t *buf;
  } test_arr[] = {
    {__LINE__, mpts_pat_header},
  };

  uint8_t buf[MPEG_TS_PACKET_SIZE];

  for (const auto &test_item : test_arr) {
    // parse binary packet to pb
    Mpeg2Ts mpeg2ts;
    mpeg2ts_parser_.ParsePacket(0, 0, test_item.buf, MPEG_TS_PACKET_SIZE,
        &mpeg2ts);

    // dump the pb back to binary
    EXPECT_EQ(MPEG_TS_PACKET_SIZE,
        mpeg2ts_parser_.DumpPacket(mpeg2ts, buf, MPEG_TS_PACKET_SIZE))
        << "line " << test_item.line;
    EXPECT_EQ(0, memcmp(buf, test_item.buf, MPEG_TS_PACKET_SIZE))
        << "line " << test_item.line;
  }
}


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
