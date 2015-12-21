// Copyright Google Inc. Apache 2.0.

#ifndef MPEG2TS_PARSER_H_
#define MPEG2TS_PARSER_H_

#include <stdint.h>  // for uint8_t, int64_t

#include "mpeg2ts.pb.h"

#define MPEG_TS_PACKET_SYNC 0x47  // 'G'
#define MPEG_TS_PACKET_SIZE 188

#define MPEG_TS_PID_PAT 0

#define MPEG_TS_TABLE_ID_PROGRAM_ASSOCIATION_SECTION 0x00
#define MPEG_TS_TABLE_ID_CONDITIONAL_ACCESS_SECTION 0x01
#define MPEG_TS_TABLE_ID_TS_PROGRAM_MAP_SECTION 0x02
#define MPEG_TS_TABLE_ID_TS_DESCRIPTION_SECTION 0x03
#define MPEG_TS_TABLE_ID_14496_SCENE_DESCRIPTION_SECTION 0x04
#define MPEG_TS_TABLE_ID_14496_OBJECT_DESCRIPTION_SECTION 0x05
#define MPEG_TS_TABLE_ID_FORBIDDEN 0xff

class Mpeg2TsParser {
 public:
  explicit Mpeg2TsParser(bool return_raw_packets);
  ~Mpeg2TsParser() {}

  // Process a binary mpeg2ts packet into a protobuf.
  // Returns the number of packets parsed, or -1 if there was an
  // error.
  int ParsePacket(int64_t pi, int64_t bi, const uint8_t *buf, int len,
      Mpeg2Ts *mpeg2ts);

  // Process a protobuf into a binary mpeg2ts packet.
  int DumpPacket(const Mpeg2Ts &mpeg2ts, uint8_t *buf, int len);

 protected:
  int ParseValidPacket(const uint8_t *buf, int len,
      Mpeg2TsPacket *mpeg2ts_packet);
  int DumpValidPacket(const Mpeg2TsPacket &mpeg2ts_packet,
      uint8_t *buf, int len);

  int ParseHeader(const uint8_t *buf, int len, Mpeg2TsHeader *mpeg2ts_header);
  int DumpHeader(const Mpeg2TsHeader &mpeg2ts_header, uint8_t *buf, int len);

  int ParseAdaptationField(const uint8_t *buf, int len,
      AdaptationField *adaptation_field);
  int DumpAdaptationField(const AdaptationField &adaptation_field,
      uint8_t *buf, int len);

  int ParseAdaptationFieldExtension(const uint8_t *buf, int len,
      AdaptationFieldExtension *adaptation_field_extension);
  int DumpAdaptationFieldExtension(
      const AdaptationFieldExtension &adaptation_field_extension,
      uint8_t *buf, int len);

  int ParsePesPacket(const uint8_t *buf, int len, PesPacket *pes_packet);
  int DumpPesPacket(const PesPacket &pes_packet, uint8_t *buf, int len);

  int ParsePCR(const uint8_t *buf, int len, PCR *pcr);
  int DumpPCR(const PCR &pcr, uint8_t *buf, int len);

  int ParseESCR(const uint8_t *buf, int len, PCR *pcr);
  int DumpESCR(const PCR &pcr, uint8_t *buf, int len);

  int ParseESRate(const uint8_t *buf, int len, int *es_rate);
  int DumpESRate(const int es_rate, uint8_t *buf, int len);

  PesPacket::StreamIdType GetStreamIdType(int stream_id);

  int ParsePTS(const uint8_t *buf, int len, int64_t *pts);
  int DumpPTS(const int64_t pts, int start, uint8_t *buf, int len);

  int ParseDsmTrickMode(const uint8_t *buf, int len,
      DsmTrickMode *dsm_trick_mode);
  int DumpDsmTrickMode(const DsmTrickMode &dsm_trick_mode,
      uint8_t *buf, int len);

  int ParsePesExtension(const uint8_t *buf, int len,
      PesExtension *pes_extension);
  int DumpPesExtension(const PesExtension &pes_extension,
      uint8_t *buf, int len);

  // PSI parsing
  int ParsePsiPacket(const uint8_t *buf, int len, PsiPacket *pes_packet);
  int DumpPsiPacket(const PsiPacket &psi_packet, uint8_t *buf, int len);

  int ParseProgramAssociationSection(const uint8_t *buf, int len,
      ProgramAssociationSection *program_association_section);
  int DumpProgramAssociationSection(
    const ProgramAssociationSection &program_association_section,
    uint8_t *buf, int len);

  int ParseProgramInformation(const uint8_t *buf, int len,
      ProgramInformation *program_information);
  int DumpProgramInformation(const ProgramInformation &program_information,
      uint8_t *buf, int len);

  int ParseProgramMapSection(const uint8_t *buf, int len,
      ProgramMapSection *program_map_section);
  int DumpProgramMapSection(const ProgramMapSection &program_map_section,
    uint8_t *buf, int len);

  int ParseStreamDescription(const uint8_t *buf, int len,
      StreamDescription *stream_description);
  int DumpStreamDescription(const StreamDescription &stream_description,
      uint8_t *buf, int len);

  int ParseOtherPsiSection(const uint8_t *buf, int len,
      OtherPsiSection *other_psi_section);
  int DumpOtherPsiSection(const OtherPsiSection &other_psi_section,
      uint8_t *buf, int len);

  int ParseDescriptor(const uint8_t *buf, int len, Descriptor *descriptor);
  int DumpDescriptor(const Descriptor &descriptor, uint8_t *buf, int len);

 private:
  const bool return_raw_packets_;
  Mpeg2TsPacket mpeg2ts_packet_;
};

#endif  // MPEG2TS_PARSER_H_
