// Copyright Google Inc. Apache 2.0.

#include <arpa/inet.h>
#include <ctype.h>
#include <getopt.h>
#include <inttypes.h>  // for PRId64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <list>
#include <map>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/text_format.h>

#include "ac3_utils.h"
#include "h264_utils.h"
#include "m2pb.h"
#include "mpeg2ts_parser.h"
#include "mpeg2ts_reader.h"
#include "mpeg2ts.pb.h"
#include "protobuf_utils.h"
#include "pts_utils.h"

typedef enum {
  ACTION_INVALID = -1,
  ACTION_NONE = 0,
  ACTION_TOTXT = 1,
  ACTION_TOBIN = 2,
  ACTION_TEST = 3,
  ACTION_DUMP = 3,
} ActionEnum;

/* default values */
#define DEFAULT_WRITE 0
#define DEFAULT_DEBUG 0

std::map<std::string, std::string> ACCESSOR_SHORTCUT_MAP = {
  {"pts", "parsed.pes_packet.pts"},
  {"pusi", "parsed.header.payload_unit_start_indicator"},
  {"pid", "parsed.header.pid"},
};

std::list<std::string> ACCESSOR_EXTRA_LIST = {
  "type", "syncframe",
};

typedef struct status_t
{
  int sync_gap;
  ActionEnum action;
  int debug;
  int ignore_pts_delta;
  int allow_raw_packets;
  int64_t pts_delta;
  int64_t pts_delta_audio;
  int64_t pts_delta_video;
  int video_pid;
  std::list<int> audio_pid_l;
  std::list<std::string> dump_fields;
  char *in;
  char *out;
  // args-only
  int nrem;
  char** rem;
} status_t;


extern char *optarg;
extern int optind, opterr, optopt;


void usage(char *name)
{
  fprintf(stderr, "usage: %s [options] <command> [in.ts] [out.ts]\n", name);
  fprintf(stderr, "where options are:\n");
  fprintf(stderr, "\t-s <sync_gap>:\t\tMaximum sync gap (%i)\n",
      DEFAULT_MAXIMUM_SYNC_GAP);
  fprintf(stderr, "\t--no-raw:\t\tPunt on raw packets\n");
  fprintf(stderr, "\t--ignore-pts-delta:\t\tIgnore pts delta values\n");
  fprintf(stderr, "\t-d:\t\tIncrease debug verbosity\n");
  fprintf(stderr, "\t-q:\t\tQuiet mode (zero debug verbosity)\n");
  fprintf(stderr, "\t-h:\t\tHelp\n");
  fprintf(stderr, "\nSome valid commands:\n");
  fprintf(stderr, "\ttotxt: convert binary representation to protobuf\n");
  fprintf(stderr, "\ttobin: convert protobuf representation to binary\n");
  fprintf(stderr, "\tdump: ipsumdump-like print\n");
  fprintf(stderr, "\t\t--<pb_field>: any Mpeg2Ts proto field\n");
  fprintf(stderr, "\t\t");
  for (auto s : ACCESSOR_SHORTCUT_MAP)
    fprintf(stderr, "--<%s>, ", s.first.c_str());
  for (auto s : ACCESSOR_EXTRA_LIST)
    fprintf(stderr, "--<%s>, ", s.c_str());
  fprintf(stderr, "\n");
  fprintf(stderr, "\ttest: test a binary file (binary->protobuf->binary)\n");
  fprintf(stderr, "\thelp: this usage\n");
}


ActionEnum GetAction(char *cmd) {
  if (strlen(cmd) == 0)
    return ACTION_NONE;
  else if (strcmp(cmd, "totxt") == 0)
    return ACTION_TOTXT;
  else if (strcmp(cmd, "tobin") == 0)
    return ACTION_TOBIN;
  else if (strcmp(cmd, "test") == 0)
    return ACTION_TEST;
  else if (strcmp(cmd, "dump") == 0)
    return ACTION_DUMP;
  else
    return ACTION_INVALID;
}


status_t *parse_args(int argc, char** argv)
{
  int arg;
  int optindex = 0;
  char *endptr;
  static status_t status;

  // default status values
  status.sync_gap = DEFAULT_MAXIMUM_SYNC_GAP;
  status.debug = DEFAULT_DEBUG;
  status.ignore_pts_delta = 0;
  status.allow_raw_packets = 1;
  status.pts_delta = 0;
  status.pts_delta_video = 0;
  status.pts_delta_audio = 0;
  status.dump_fields.clear();

  struct option longopts[] = {
    // options with no argument
    {"debug", no_argument, NULL, 'd'},
    {"ignore-pts-delta", no_argument, &status.ignore_pts_delta, 1},
    {"no-raw", no_argument, &status.allow_raw_packets, 0},
    // matching options to short options
    {"sync-gap", required_argument, NULL, 's'},
    {"help", no_argument, NULL, 'h'},
    {"quiet", no_argument, NULL, 'q'},
    {NULL, 0, NULL, 0},
  };

  while ((arg = getopt_long(argc, argv, ":dqs:", longopts, &optindex)) != -1) {
    switch (arg) {
      case 0:  // long options
        break;

      case 's':
        status.sync_gap = strtol(optarg, &endptr, 0);
        if (*endptr != '\0') {
          usage(argv[0]);
          exit(-1);
        }
        // check the value is OK
        if (status.sync_gap < SYNC_GAP_MINIMUM) {
          fprintf(stderr, "error: sync_gap (%i) must be at least %i\n",
              status.sync_gap, SYNC_GAP_MINIMUM);
          exit(-1);
        }
        if (status.sync_gap > SYNC_GAP_MAXIMUM) {
          fprintf(stderr, "error: sync_gap (%i) must be at most %i\n",
              status.sync_gap, SYNC_GAP_MAXIMUM);
          exit(-1);
        }
        break;

      case 'd':
        status.debug += 1;
        break;

      case 'q':
        status.debug = -1;
        break;

      case '?':
        {
        // check for ipsumdump-like arguments
        std::string s(argv[optind-1]+2);
        // check for shortcut accessor values
        auto iter = ACCESSOR_SHORTCUT_MAP.find(s);
        if (iter != ACCESSOR_SHORTCUT_MAP.end()) {
          status.dump_fields.push_back(iter->second);
          continue;
        }
        // check for extra accessor values
        if (std::find(ACCESSOR_EXTRA_LIST.begin(),
            ACCESSOR_EXTRA_LIST.end(), s) !=
            ACCESSOR_EXTRA_LIST.end()) {
          status.dump_fields.push_back(s);
          continue;
        }
        // check for default values
        Mpeg2Ts mpeg2ts;
        const google::protobuf::Descriptor* descriptor =
            mpeg2ts.GetDescriptor();
        if (field_exists(descriptor, s)) {
          status.dump_fields.push_back(s);
          continue;
        }
        // invalid option
        fprintf(stderr, "error: unrecognized option: %s\n", argv[optind-1]);
        exit(-1);
        break;
        }

      case 'h':
      default:
        usage(argv[0]);
        exit(0);
        break;

      }
    }

  /* require a valid command */
  if (argc - optind < 1) {
    fprintf(stderr, "error: need valid command\n");
    usage(argv[0]);
    exit(-1);
  }
  status.action = GetAction(argv[optind++]);
  if (status.action == ACTION_INVALID) {
    fprintf(stderr, "error: invalid action: \"%s\"\n", argv[optind-1]);
    usage(argv[0]);
    exit(-1);
  }

  /* check for an in file */
  if (argc - optind < 1) {
    // use stdin
    status.in = NULL;
  } else {
    status.in = argv[optind++];
    /* check for an out file */
    if (argc - optind < 1) {
      // use stdout
      status.out = NULL;
    } else {
      status.out = argv[optind++];
    }
  }

  /* store remaining arguments */
  status.nrem = argc - optind;
  status.rem = argv + optind;

  return &status;
}


char *EscapeBinary(const uint8_t *buffer, const int len)
{
  static char out[2 * 1024];
  int oi = 0;
  for (int bi = 0; (bi < len) && (bi < ((int)sizeof(out) - 3)); ++bi) {
    int first = ((buffer[bi] & 0xf0) >> 4);
    out[oi++] = first + ((first < 10) ? '0' : 'W');  // 'W' = 'a' - 10
    int second = (buffer[bi] & 0x0f);
    out[oi++] = second + ((second < 10) ? '0' : 'W');  // 'W' = 'a' - 10
  }
  out[oi] = '\0';
  return out;
}


int CheckTestResults(const uint8_t *inbuf, const int inlen,
    const uint8_t *outbuf, const int outlen, const Mpeg2Ts &mpeg2ts,
    status_t *status) {
  int res = 0;
  if (inlen != outlen) {
    fprintf(stderr, "error: different lengths in: %i != out: %i\n",
        inlen, outlen);
    res = -1;
  } else if (memcmp(inbuf, outbuf, inlen) != 0) {
    fprintf(stderr, "error: different contents:\n");
    res = -1;
  } else if (!status->allow_raw_packets && mpeg2ts.has_raw()) {
    fprintf(stderr, "error: raw packets disabled:\n");
    res = -1;
  }

  if (res < 0) {
    fprintf(stderr, "   in: \"%s\"\n", EscapeBinary(inbuf, inlen));
    fprintf(stderr, "  parsed: \"%s\"\n",
        mpeg2ts.ShortDebugString().c_str());
    fprintf(stderr, "  out: \"%s\"\n", EscapeBinary(outbuf, outlen));
    // TODO(chema): remove this (?)
    FILE *pFile;
    pFile = fopen("/tmp/in", "wb");
    fwrite (inbuf, sizeof(int8_t), inlen, pFile);
    fclose(pFile);
    pFile = fopen("/tmp/1", "wb");
    std::string eb1 = EscapeBinary(inbuf, inlen);
    fwrite(eb1.c_str(), sizeof(char), eb1.length(), pFile);
    fclose(pFile);
    pFile = fopen("/tmp/out", "wb");
    fwrite(outbuf, sizeof(int8_t), outlen, pFile);
    fclose(pFile);
    pFile = fopen("/tmp/2", "wb");
    std::string eb2 = EscapeBinary(outbuf, outlen);
    fwrite(eb2.c_str(), sizeof(char), eb2.length(), pFile);
    fclose(pFile);
  }
  return res;
}


void DumpLine(const Mpeg2Ts &mpeg2ts, status_t *status, FILE* fout) {
  char buf[1024] = {0};
  int bi = 0;
  for (auto &s : status->dump_fields) {
    // implement the extra accessors
    if (s == "type") {
      char stype = '-';
      if (mpeg2ts.parsed().header().has_pid()) {
        int pid = mpeg2ts.parsed().header().pid();
        if (pid == status->video_pid) {
          // video
          int frame_type = -1;
          if (mpeg2ts.parsed().has_data_bytes()) {
            const uint8_t *data = reinterpret_cast<const uint8_t *>(
                mpeg2ts.parsed().data_bytes().c_str());
            int len = mpeg2ts.parsed().data_bytes().length();
            frame_type = h264_frame_type(data, len);
            stype = (frame_type == 1) ? 'I' :
                ((frame_type == 2) ? 'P' :
                ((frame_type == 3) ? 'B' :
                ((frame_type == 4) ? 'V' : '-')));
          }
        } else if (std::find(status->audio_pid_l.begin(),
            status->audio_pid_l.end(), pid) != status->audio_pid_l.end()) {
          if (mpeg2ts.parsed().pes_packet().has_pts()) {
            auto iter = std::find(status->audio_pid_l.begin(),
                status->audio_pid_l.end(), pid);
            int position = distance(status->audio_pid_l.begin(), iter);
            stype = '1' + position;
          }
        }
      }
      bi += snprintf(buf+bi, sizeof(buf)-bi, "%c ", stype);
    } else if (s == "syncframe") {
      int syncframe_distance = -1;
      if (mpeg2ts.parsed().header().has_pid()) {
        int pid = mpeg2ts.parsed().header().pid();
        if (std::find(status->audio_pid_l.begin(),
            status->audio_pid_l.end(), pid) != status->audio_pid_l.end()) {
          // audio
          if (mpeg2ts.parsed().has_data_bytes()) {
            const uint8_t *data = reinterpret_cast<const uint8_t *>(
                mpeg2ts.parsed().data_bytes().c_str());
            int len = mpeg2ts.parsed().data_bytes().length();
            syncframe_distance = ac3_syncframe_distance(data, len);
          }
        }
      }
      if (syncframe_distance != -1) {
        bi += snprintf(buf+bi, sizeof(buf)-bi, "%i ", syncframe_distance);
      } else {
        bi += snprintf(buf+bi, sizeof(buf)-bi, "- ");
      }
    } else {
      // known protobuf field
      std::string value;
      if (get_field_value(mpeg2ts, s, &value)) {
        bi += snprintf(buf+bi, sizeof(buf)-bi, "%s ", value.c_str());
      } else {
        bi += snprintf(buf+bi, sizeof(buf)-bi, "- ");
      }
    }
  }

  fprintf(fout, "%s\n", buf);
  return;
}


std::list<int> MPEGTS_VIDEO_STREAM_TYPE = {
  // ISO/IEC 11172 Video
  0x01,
  // ITU-T Rec. H.262 | ISO/IEC 13818-2 Video or ISO/IEC 11172-2 constrained
  // parameter video stream
  0x02,
  // H.264/14496-10 video (MPEG-4/AVC)
  0x1b,
};

std::list<int> MPEGTS_AUDIO_STREAM_TYPE = {
  // ISO/IEC 11172 Audio
  0x03,
  // ISO/IEC 13818-3 Audio
  0x04,
  // 13818-7 Audio with ADTS transport syntax
  0x0f,
  // ISO/IEC 14496-2 Visual
  0x10,
  // ISO/IEC 14496-3 Audio with the LATM transport syntax as defined
  // in ISO/IEC 14496-3 / AMD 1
  0x11,
  // User private (commonly Dolby/AC-3 in ATSC)
  0x81,
};

void mpegts_process_packet(const Mpeg2Ts &mpeg2ts, status_t *status) {
  // look for PMT packets
  if (mpeg2ts.parsed().psi_packet().program_map_section_size() == 0)
    return;
  status->audio_pid_l.clear();
  for (int i = 0; i < mpeg2ts.parsed().psi_packet().program_map_section_size();
      ++i) {
    auto &program_map_section =
        mpeg2ts.parsed().psi_packet().program_map_section(i);
    for (int j = 0; j < program_map_section.stream_description_size(); ++j) {
      auto &stream_description = program_map_section.stream_description(j);
      //fprintf(stdout, "%s\n", stream_description.ShortDebugString().c_str());
      if (std::find(MPEGTS_VIDEO_STREAM_TYPE.begin(),
          MPEGTS_VIDEO_STREAM_TYPE.end(), stream_description.stream_type()) !=
            MPEGTS_VIDEO_STREAM_TYPE.end()) {
        status->video_pid = stream_description.elementary_pid();
      } else if (std::find(MPEGTS_AUDIO_STREAM_TYPE.begin(),
          MPEGTS_AUDIO_STREAM_TYPE.end(), stream_description.stream_type()) !=
            MPEGTS_AUDIO_STREAM_TYPE.end()) {
        status->audio_pid_l.push_back(stream_description.elementary_pid());
      }
    }
  }
}


int mpegts_read_binary(status_t *status) {
  FILE *fin = stdin;
  if (status->in != NULL && (strcmp(status->in, "-") != 0)) {
    /* open in file */
    fin = fopen(status->in, "r");
    if (fin == NULL) {
      fprintf(stderr, "error: cannot open in file: %s\n", status->in);
      return -1;
    }
  }

  FILE *fout = stdout;
  if (status->out != NULL && (strcmp(status->out, "-") != 0)) {
    /* open out file */
    fout = fopen(status->out, "w+");
    if (fout == NULL) {
      fprintf(stderr, "error: cannot open out file: %s\n", status->out);
      return -1;
    }
  }

  /* create mpeg2ts objects */
  Mpeg2TsReader mpeg2ts_reader(fin, status->debug);
  Mpeg2TsParser mpeg2ts_parser(true);
  Mpeg2Ts mpeg2ts;

  uint8_t *buf;
  int len;
  int64_t pi;
  int64_t bi;
  while ((len = mpeg2ts_reader.GetChunk(&buf, &pi, &bi)) > 0) {
    len = mpeg2ts_parser.ParsePacket(pi, bi, buf, len, &mpeg2ts);
    // check whether the packet is interesting
    mpegts_process_packet(mpeg2ts, status);
    if (status->action == ACTION_TOTXT)
      fprintf(fout, "%s\n", mpeg2ts.ShortDebugString().c_str());
    else if (status->action == ACTION_DUMP)
      DumpLine(mpeg2ts, status, fout);
    else if (status->action == ACTION_TEST) {
      uint8_t out[MPEG_TS_PACKET_SIZE];
      int outlen = mpeg2ts_parser.DumpPacket(mpeg2ts, out, sizeof(out));
      if (CheckTestResults(buf, len, out, outlen, mpeg2ts, status))
        return -1;
    }
    mpeg2ts_reader.Next(len);
  }

  /* close in/out files */
  fclose(fin);
  fclose(fout);

  if (len < 0) {
    // lost sync
    fprintf(stderr, "error: lost sync of %s at byte %" PRId64 "\n",
        status->in != NULL ? status->in : "stdin", bi);
    return -1;
  }
  return 0;
}


int mpegts_read_text(status_t *status) {
  FILE *fin = stdin;
  if (status->in != NULL && (strcmp(status->in, "-") != 0)) {
    /* open in file */
    fin = fopen(status->in, "r");
    if (fin == NULL) {
      fprintf(stderr, "error: cannot open in file: %s\n", status->in);
      return -1;
    }
  }

  FILE *fout = stdout;
  if (status->out != NULL && (strcmp(status->out, "-") != 0)) {
    /* open out file */
    fout = fopen(status->out, "w+");
    if (fout == NULL) {
      fprintf(stderr, "error: cannot open out file: %s\n", status->out);
      return -1;
    }
  }

#define MAX_LINE_SIZE 10240
  size_t blen = MAX_LINE_SIZE;
  char *line = (char *)malloc(blen * sizeof(char));
  size_t len = 0;
  ssize_t slen = 0;

  Mpeg2TsParser mpeg2ts_parser(true);
  Mpeg2Ts mpeg2ts;

  while ((slen = getline(&line, &blen, fin)) != -1) {
    len = slen;
    // TODO(chema): support multi-line text protobuf
    if (status->debug > 3) {
      printf("--------\nRetrieved line of length %zu :\n", len);
      printf("%s", line);
    }
    std::string bi(line, len);
    if (!google::protobuf::TextFormat::ParseFromString(bi, &mpeg2ts)) {
      printf("Failed to parse line into protobuf: \"%s\"\n", line);
      FILE *pFile = fopen("/tmp/in", "wb");
      fwrite (bi.c_str(), sizeof(char), bi.length(), pFile);
      fclose(pFile);
      exit(-1);
    }
    // write binary protobuf
    uint8_t out[MPEG_TS_PACKET_SIZE];
    int outlen = mpeg2ts_parser.DumpPacket(mpeg2ts, out, sizeof(out));
    fwrite(out, outlen, sizeof(char), fout);
  }

  /* close in/out files */
  fclose(fin);
  fclose(fout);

  return 0;
}


int main(int argc, char** argv)
{
  status_t *status;
  int i;

  /* parse args */
  status = parse_args(argc, argv);
  if (status == NULL) {
    usage(argv[0]);
    exit(-1);
  }

  /* print args */
  if (status->debug > 1) {
    printf("status->action = %i\n", status->action);
    printf("status->debug = %i\n", status->debug);
    printf("status->sync_gap = %i\n", status->sync_gap);
    printf("status->ignore_pts_delta = %i\n", status->ignore_pts_delta);
    printf("status->allow_raw_packets = %i\n", status->allow_raw_packets);
    printf("status->nrem = %i\n", status->nrem);
    for (i=0; i<status->nrem; ++i)
      printf("status->rem[%i] = %s\n", i, status->rem[i]);
  }

  if ((status->action == ACTION_TOTXT) ||
      (status->action == ACTION_TEST) ||
      (status->action == ACTION_DUMP))
    return mpegts_read_binary(status);

  if (status->action == ACTION_TOBIN)
    return mpegts_read_text(status);

  return 0;
}

