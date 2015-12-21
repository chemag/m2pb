#!/usr/bin/env python

# Copyright Google Inc. Apache 2.0.

import argparse
import datetime
import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy
import os.path
import pandas as pd
import pts_utils
import re
import subprocess
import sys


from google.protobuf import text_format

sys.path.append('../src')
import mpeg2ts_pb2


M2PB = 'm2pb'

## axes.formatter.useoffset not in 1.3.1
#mpl.rcParams['axes.formatter.useoffset'] = False

DEFAULT_PACKET_LENGTH = 10000

PTS_PER_FRAME = 3003

# TODO(chema): remove this once we can detect the latest I-frame
SPLICE_BUFFER_FRAMES = 20  # 20 frames should always work

# necessary to move decoder frame to the "decode but not present" zone
PTS_DELTA_DONT_PRESENT = -pts_utils.milliseconds_to_pts(100)  # 100 ms

def frames_to_pts(frames):
  return frames * PTS_PER_FRAME


def get_opts(argv):
  # init parser
  parser = argparse.ArgumentParser(description='Parse mpeg-ts file.')
  # usage = 'usage: %prog [options] arg1 arg2'
  # parser = argparse.OptionParser(usage=usage)
  # parser.print_help() to get argparse.usage (large help)
  # parser.print_usage() to get argparse.usage (just usage line)
  parser.add_argument('-d', '--debug', dest='debug', default=0,
      action='count',
      help='Increase verbosity (specify multiple times for more)')
  parser.add_argument('--quiet', action='store_const',
      dest='debug', const=-1,
      help='Zero verbosity',)
  parser.add_argument('--simple', action='store_const',
      dest='simple', const=True, default=False,
      help='Do simple video filtering',)
  parser.add_argument('-i', '--input', action='append',
      dest='input_file_spec', default=[],
      metavar='INPUT_FILE_SPEC',
      help='input file specification (file:splice_in:splice_out)',)
  parser.add_argument('-o', '--output', action='store',
      dest='output_filename', default='-',
      metavar='OUTPUT_FILENAME',
      help='output filename',)
  parser.add_argument('--splice-frames', action='store',
      dest='splice_frames', default=SPLICE_BUFFER_FRAMES,
      type=float,
      metavar='SPLICE_FRAMES',
      help='explicit splice buffer length (in frames)',)
  parser.add_argument('-v', '--version', action='version',
      version='%(prog)s 1.0')
  # non-opt arguments must be input files
  parser.add_argument('remaining', nargs=argparse.REMAINDER)
  return parser.parse_args(argv[1:])


videostr_pid = 481
audiostr_pid_d = { 482: 1, 483: 2 }
pmtstr = ' program_map_section {'

video_stream_type_l = [
    0x1b,  # H.264/14496-10 video (MPEG-4/AVC)
]

audio_stream_type_l = [
    0x0f,  # 13818-7 Audio with ADTS transport syntax
    0x81,  # User private (commonly Dolby/AC-3 in ATSC)
]


headerre = r"""
    packet:\s(?P<packet>[0-9]+)
    \s
    byte:\s(?P<byte>[0-9]+)
    \s
    parsed\s{
    \s
    header\s{
      [^}]*
      \s
      payload_unit_start_indicator:\s(?P<pusi>(true|false))
      \s
      [^}]*
      \s
      pid:\s(?P<pid>[0-9]+)
      \s
      [^}]*
      \s
    }
    (?P<rem>.*)
"""


# PAT
# packet: 0 byte: 0 parsed { header { transport_error_indicator: false payload_unit_start_indicator: true transport_priority: false pid: 0 transport_scrambling_control: 0 adaptation_field_exists: false payload_exists: true continuity_counter: 0 } psi_packet { pointer_field: "" program_association_section { table_id: 0 transport_stream_id: 1 version_number: 0 current_next_indicator: true section_number: 0 last_section_number: 0 program_information { program_number: 1 program_map_pid: 480 } crc_32: 760248324 } } }

# PMT
# packet: 1 byte: 188 parsed { header { transport_error_indicator: false payload_unit_start_indicator: true transport_priority: false pid: 480 transport_scrambling_control: 0 adaptation_field_exists: false payload_exists: true continuity_counter: 0 } psi_packet { pointer_field: "" program_map_section { table_id: 2 program_number: 1 version_number: 0 current_next_indicator: true section_number: 0 last_section_number: 0 pcr_pid: 481 mpegts_descriptor { tag: 14 length: 3 data: "\300<x" } stream_description { stream_type: 27 elementary_pid: 481 mpegts_descriptor { tag: 40 length: 4 data: "M@(?" } mpegts_descriptor { tag: 14 length: 3 data: "\300:\230" } } stream_description { stream_type: 129 elementary_pid: 482 mpegts_descriptor { tag: 5 length: 4 data: "AC-3" } mpegts_descriptor { tag: 129 length: 7 data: "\006(\005\377\037\001?" } mpegts_descriptor { tag: 10 length: 4 data: "und\000" } mpegts_descriptor { tag: 14 length: 3 data: "\300\001\340" } } crc_32: 1966564032 } } }
pmtre = r"""
    parsed
    .*?
    header\s{
      \s
      .*?
      \s
      pid:\s(?P<pid>[0-9]+)
      \s
      .*?
      \s
      program_map_section\s{
      .*?
    (?P<rem>stream_description\s{.*)
"""

pmtstreamre = r"""
    stream_description\s{
      \s
      stream_type:\s(?P<stream_type>[0-9]+)
      \s
      elementary_pid:\s(?P<elementary_pid>[0-9]+)
      \s
      [^}]*
    }
    (?P<rem>.*)
"""


# video
# packet: 2 byte: 376 parsed { header { transport_error_indicator: false payload_unit_start_indicator: true transport_priority: false pid: 481 transport_scrambling_control: 0 adaptation_field_exists: true payload_exists: true continuity_counter: 1 } adaptation_field { adaptation_field_length: 7 discontinuity_indicator: true random_access_indicator: true elementary_stream_priority_indicator: true splicing_point_flag: false transport_private_data_flag: false pcr { base: 18039 extension: 23 } } pes_packet { stream_id: 224 stream_id_type: STREAM_ID_VIDEO_13818 pes_packet_length: 0 pes_scrambling_control: 0 pes_priority: 0 data_alignment_indicator: true copyright: false original_or_copy: false pes_header_data_length: 10 pts: 183003 dts: 180000 pes_packet_data_byte: "\000\000\000\001\t\020\000\000\000\001\'M@(\344`<\002#\357\001\020\000\000>\220\000\016\246\016(\000\002\334l\000\005\270\336\367\270\017\204B)\300\000\000\000\001(\372K\310\000\000\000\001\006\000\007\201<h\000#(@\200\000\000\000\001\006\001\007\000\000\003\000\000\003\000\0022\200\000\000\000\001\006\005H\217\273lt|>Ox\237\007\214\263]<\027~Elemental Video Engine(tm) www.elementaltechnolo" } }
# packet: 3 byte: 564 parsed { header { transport_error_indicator: false payload_unit_start_indicator: false transport_priority: false pid: 481 transport_scrambling_control: 0 adaptation_field_exists: false payload_exists: true continuity_counter: 2 } data_bytes: "gies.com\200\000\000\000\001%\210\201\000@-_\376\367\261\217\201M\307Y\r\2279T\234j\327\222\335\003\272\331\350\'\365\036\237\365\026IJikJB\323\303\220\353\252\216m \003\373\035\242\315\322\213/\254\202r\220\232M&\223I\244\322i4\232NM\007\275\230\355\207\311\354\\\261%\327:>za\334\202\027\325u\372\216\361\357;\366\230\026\237\317\347\363\371\374\376\177?\237\317\351Q\006\212\305b\261X\254V+\025\212\305b\261X\254V+\025\212\305b\261X\254V+\025\212\305b\261YU-E\275\026\364[\321oE\275\026\364[\321oE\275\026\364[\321oE\275" }

videore = r"""
    .*?
    pes_packet\s{
      [^}]*
      \spts:\s(?P<pts>[0-9]+)
"""

# audio
# packet: 8116 byte: 1525808 parsed { header { transport_error_indicator: false payload_unit_start_indicator: true transport_priority: false pid: 482 transport_scrambling_control: 0 adaptation_field_exists: false payload_exists: true continuity_counter: 1 } pes_packet { stream_id: 189 stream_id_type: STREAM_ID_PRIVATE_STREAM_1 pes_packet_length: 1544 pes_scrambling_control: 0 pes_priority: 0 data_alignment_indicator: true copyright: false original_or_copy: false pes_header_data_length: 5 pts: 183003 pes_packet_data_byte: "\013w\034\014\0240C\037\3677$\222\000\000\336l[\204\004\004\004\020\200\200\200\202\n\343\347\317\237>|\371\363\347\317\237>|\371\363\347\317\237>|\371\363\347\317\237>|\371\363\347\317\237\177\316\257\237>|\371\363\347\317\237>|\371\363\347\317\237>|\371\363\347\317\237>|\371\363\347\317\237\177\363\253\347\317\237>|\371\363\347\317\237>|\371\363\347\317\237>|\371\363\347\317\237>|\371\363\347\336S\342H\221$\000\000\000\000\003\306\333m\266\333\307\217\036;\273\273\270\000\000\000\000\000\000\000\000\000\000\000\356\356\356\356\356\356\333m\266\333o\2375" } }
# packet: 8148 byte: 1531824 parsed { header { transport_error_indicator: false payload_unit_start_indicator: false transport_priority: false pid: 482 transport_scrambling_control: 0 adaptation_field_exists: false payload_exists: true continuity_counter: 2 } data_bytes: "\255kZ\326\265\255kZ\326\265\255kZ\326\265\240\000\000\000\033m\266\333o\036<x\356\356\356\340\000\000\000\000\000\000\000\000\000\000\003\273\273\273\273\273\273m\266\333m\276|\326\265\255kZ\326\230\000\000\000\000\000\007\215\266\333m\267\217\036<wwwp\000\000\000\000\000\000\000\000\000\000\001\335\335\335\335\335\335\266\333m\266\337>kZ\326\265\255kZ\326\265\255kZ\326\265\255k@\000\000\0006\333m\266\336<x\361\335\335\335\300\000\000\000\000\000\000\000\000\000\000\007wwwwwv\333m\266\333|\371\255kZ\326\265\2550\000\000\000\000\000\017\033m\266\333o\036<x\356\356\356\340\000\000" }

ptsre = r"""
    .*?
    pes_packet\s{
      [^}]*
      \spes_packet_length:\s(?P<pes_packet_length>[0-9]+)
      [^}]*
      \spts:\s(?P<pts>[0-9]+)
"""

scte35re = 'pid: 0x01ea pusi:'






# scte35
# == byte: 2396328088 packet: 12746427 ts_header { pid: 0x01ea pusi: 1 stream_type: 0x86 stream_type_str: "User private" } pes_header { start_code: 00fc30 stream_id: 0x20 stream_id_str: "Slice, vertical posn 32 " pes_packet_length: 0 type: "MPEG-1 packet layer packet" } data { len: 165 bytes: 7fcffececfe61c00000000000048bc26b5ffffff... }

# other
# == byte: 448634364 packet: 2386354 ts_header { pid: 0x1ffe pusi: 1 stream_type: 0x06 stream_type_str: "H.222.0/13818-1 PES private data (maybe Dolby/AC-3 in DVB)" } pes_header { start_code: 000001 stream_id: 0xf0 stream_id_str: "SYSTEM START: ECM stream " pes_packet_length: 176 type: "MPEG-1 packet layer packet"error: "MPEG-1 PES packet has 0x0X instead of 0x40, 0x2X, 0x3X or 0x0F" } data { len: 165 bytes: 8900000001000027100200017700e86d1486083d... }


#def get_pts_list(input_file, input_pts):
#  command = [M2PB, "--packet", "--pts", "--pusi", "--pid", "--type",
#      "dump", input_file]
#  proc = subprocess.Popen(command, stdout=subprocess.PIPE)
#  last_i_frame = pts_utils.kPtsInvalid
#  for line in iter(proc.stdout.readline,''):
#    l = line.rstrip()
#    parts = l.split(' ')
#    packet = long(parts[0])
#    pts = long(parts[1]) if parts[1] != '-' else '-'
#    pusi = bool(parts[2])
#    pid = int(parts[3])
#    video_type = parts[4]
#    if pid != 481:
#      continue
#    # keep the last pts
#    if video_type == 'I':
#      if last_i_frame == pts_utils.kPtsInvalid:
#        last_i_frame = pts
#      if pts > last_i_frame == pts_utils.kPtsInvalid:



def adjust_pts_delta(l, pts_delta):
  packet = mpeg2ts_pb2.Mpeg2Ts()
  text_format.Merge(l, packet)
  if packet.parsed.pes_packet.HasField('pts'):
    # fix pts field
    packet.parsed.pes_packet.pts = pts_utils.pts_add(
        packet.parsed.pes_packet.pts, pts_delta)
  if packet.parsed.pes_packet.HasField('dts'):
    # fix dts field
    packet.parsed.pes_packet.dts = pts_utils.pts_add(
        packet.parsed.pes_packet.dts, pts_delta)
  if packet.parsed.adaptation_field.pcr.HasField('base'):
    # fix pcr.base field
    packet.parsed.adaptation_field.pcr.base = pts_utils.pts_add(
        packet.parsed.adaptation_field.pcr.base, pts_delta)
  return text_format.MessageToString(packet, as_one_line=True)


def parse_input_file_spec(input_file_spec):
  pts1 = pts_utils.kPtsInvalid
  pts2 = pts_utils.kPtsInvalid
  fname = '-'
  if ':' in input_file_spec:
    fname, pts1 = input_file_spec.split(':', 1)
    if ':' in pts1:
      try:
        pts1, pts2 = pts1.split(':', 1)
      except ValueError:
        return -1, '', 0, 0
    pts1 = long(pts1) if pts1 else pts_utils.kPtsInvalid
    pts2 = long(pts2) if pts2 else pts_utils.kPtsInvalid
  if fname != '-':
    # ensure file exists
    if not os.path.isfile(fname):
      return -1, '', 0, 0
  return 0, fname, pts1, pts2


(STATE_PRE_IN,
STATE_BUFFER_IN,
STATE_BUFFER_IN2,
STATE_THROUGH,
STATE_BUFFER_OUT,
STATE_POST_OUT) = range(6)

def get_state_str(state):
  if state == STATE_PRE_IN:
    return 'pre_in'
  elif state == STATE_BUFFER_IN:
    return 'buf_in'
  elif state == STATE_BUFFER_IN2:
    return 'buf_in2'
  elif state == STATE_THROUGH:
    return 'through'
  elif state == STATE_BUFFER_OUT:
    return 'buf_out'
  elif state == STATE_POST_OUT:
    return 'post_out'

# per input_file_specs
#
#                       pts1                        pts2
#    PRE_IN    BUFFER_IN   BUFFER_IN2      THROUGH    BUFFER_OUT   POST_OUT
# -----------]-----------[-----------]---------------[-----------]---------
#             <--10 fr--> <--10 fr-->                 <--10 fr-->
def get_state(pts, pts1, pts2, splice_buffer_pts):
  if pts1 == pts_utils.kPtsInvalid and pts2 == pts_utils.kPtsInvalid:
    return STATE_THROUGH
  if (pts1 != pts_utils.kPtsInvalid and pts2 != pts_utils.kPtsInvalid and
      pts_utils.pts_cmp(pts1, pts2) >= 0):
    # invalid case: pts1 >= pts2
    print 'invalid input file spec: pts1 > pts2 (%i > %i)' % (pts1, pts2)
    sys.exit(-1)
  if (pts1 != pts_utils.kPtsInvalid and
      pts_utils.pts_cmp(pts, pts_utils.pts_sub(pts1, splice_buffer_pts)) < 0):
    return STATE_PRE_IN
  if (pts1 != pts_utils.kPtsInvalid and
      pts_utils.pts_cmp(pts, pts_utils.pts_sub(pts1, splice_buffer_pts)) >= 0
      and pts_utils.pts_cmp(pts, pts1) < 0):
    return STATE_BUFFER_IN
  if (pts1 != pts_utils.kPtsInvalid and
      pts_utils.pts_cmp(pts, pts1) >= 0 and
      pts_utils.pts_cmp(pts, pts_utils.pts_add(pts1, splice_buffer_pts)) <= 0):
    return STATE_BUFFER_IN2
  if (pts2 != pts_utils.kPtsInvalid and
      pts_utils.pts_cmp(pts, pts2) >= 0 and
      pts_utils.pts_cmp(pts, pts_utils.pts_add(pts2, splice_buffer_pts)) <= 0):
    return STATE_BUFFER_OUT
  if (pts2 != pts_utils.kPtsInvalid and
      pts_utils.pts_cmp(pts, pts_utils.pts_add(pts2, splice_buffer_pts)) > 0):
    return STATE_POST_OUT
  # default is through
  return STATE_THROUGH


def splice_streams(input_file_specs, output_file, simple_splice, debug,
    splice_buffer_pts):
  if debug > 1:
    print 'splice_streams(%r, %s, %s, %i, %i)' % (input_file_specs,
        output_file, simple_splice, debug, splice_buffer_pts)

  # open the output command
  command_out = [M2PB, 'tobin', '-', output_file]
  proc_out = subprocess.Popen(command_out, stdin=subprocess.PIPE)
  # farthest pts of the previous file
  pts0 = pts_utils.kPtsInvalid
  # init total pts_delta
  pts_delta = pts_utils.kPtsInvalid
  for input_file_spec in input_file_specs:
    # init local pts_delta
    pts_delta_cur = pts_utils.kPtsInvalid
    _, fname, pts1, pts2 = parse_input_file_spec(input_file_spec)
    if debug > 0:
      print '-----------%s:%i:%i' % (fname, pts1, pts2)
    # open the input file command
    command_in = [M2PB, 'totxt', fname]
    proc_in = subprocess.Popen(command_in, stdout=subprocess.PIPE)
    # get the last pts
    last_video_pts = pts_utils.kPtsInvalid
    last_pts_d = {}
    farthest_video_pts = pts_utils.kPtsInvalid
    # clean up packet buffer
    packet_buffer = []
    for line in iter(proc_in.stdout.readline,''):
      l = line.rstrip()
      # use simpler comparison for faster parsing
      parts = l.split(' ')
      if parts[0] != 'packet:' or len(parts) < 16:
        print '#invalid line: %s' % l
        continue
      packet = long(parts[1])
      pid = int(parts[15])
      pusi = parts[11] == 'true'

      must_forward = True
      if pid == videostr_pid or pid not in audiostr_pid_d.keys():
        # get the pts value
        pts = pts_utils.kPtsInvalid
        if pusi:
          pts_match = re.search(ptsre, l, re.X)
          if pts_match:
            pts = long(pts_match.group('pts'))
            last_video_pts = last_pts_d[pid] = pts
        if pts == pts_utils.kPtsInvalid:
          if pid in last_pts_d:
            pts = last_pts_d[pid]
          else:
            pts = last_video_pts
        # check the location
        state = get_state(pts, pts1, pts2, splice_buffer_pts)
        if (debug > 1 and pusi) or (debug > 3):
          if pid == videostr_pid:
            who = 'video'
          else:
            who = 'others'
          print '%s: %s %s PES%s at %s' % (fname, get_state_str(state),
              who, '*' if pusi else '', pts)
        if state == STATE_PRE_IN:
          # dump the packet buffer
          packet_buffer = []
          must_forward = False
        elif state == STATE_BUFFER_IN:
          if not simple_splice:
            # buffer the packet after moving it to the do-no-present zone
            packet_buffer.append(adjust_pts_delta(l, PTS_DELTA_DONT_PRESENT))
          must_forward = False
        elif state == STATE_BUFFER_IN2 or state == STATE_THROUGH:
          # re-calculate last video pts value
          if pid == videostr_pid:
            # store the pts as "farthest pts value"
            farthest_video_pts = pts_utils.pts_max(farthest_video_pts, pts)
          # ensure we have a valid local delta
          if (pts_delta_cur == pts_utils.kPtsInvalid and
              pid == videostr_pid and
              pts0 != pts_utils.kPtsInvalid):
            if pts1 != pts_utils.kPtsInvalid:
              # calculate the delta using the pts that the user requested
              pts_delta_cur = pts_utils.pts_diff(pts0, pts1)
            else:
              # calculate the delta using the first pts seen
              pts_delta_cur = pts_utils.pts_diff(pts0, pts)
            pts_delta_cur = pts_utils.pts_add(pts_delta_cur, PTS_PER_FRAME)
            if pts_delta == pts_utils.kPtsInvalid:
              pts_delta = pts_delta_cur
            else:
              pts_delta = pts_utils.pts_add(pts_delta, pts_delta_cur)
            if debug > 0:
              print '--------------2- pts_delta: %i' % (pts_delta)
          if state == STATE_BUFFER_IN2 and not simple_splice:
            # flush packet buffer
            for ll in packet_buffer:
              # apply pts_delta
              if pts_delta != pts_utils.kPtsInvalid:
                ll = adjust_pts_delta(ll, pts_delta)
              proc_out.stdin.write(ll + '\n')
          # dump the packet buffer
          packet_buffer = []
          # apply pts_delta
          if pts_delta != pts_utils.kPtsInvalid:
            l = adjust_pts_delta(l, pts_delta)
          must_forward = True
        elif state == STATE_BUFFER_OUT:
          if not simple_splice:
            # buffer the packet after moving it to the do-no-present zone
            packet_buffer.append(adjust_pts_delta(l, PTS_DELTA_DONT_PRESENT))
          must_forward = False
        elif state == STATE_POST_OUT:
          # dump the packet buffer
          packet_buffer = []
          must_forward = False

      #elif pmtstr in l:
      #  #parse_pmt(l)
      #  must_forward = True

      else:
        # audio and other streams
        # get the pts value
        pts = pts_utils.kPtsInvalid
        if pusi:
          pts_match = re.search(ptsre, l, re.X)
          if pts_match:
            pts = long(pts_match.group('pts'))
            last_pts_d[pid] = pts
        if pts == pts_utils.kPtsInvalid:
          if pid in last_pts_d:
            pts = last_pts_d[pid]
          else:
            pts = last_video_pts
        # check the location
        state = get_state(pts, pts1, pts2, splice_buffer_pts)
        if (debug > 1 and pusi) or (debug > 3):
          print '%s: %s audio PES%s at %s' % (fname,
              get_state_str(state), '*' if pusi else '', pts)
        if state == STATE_PRE_IN or state == STATE_BUFFER_IN:
          must_forward = False
        elif state == STATE_BUFFER_IN2 or state == STATE_THROUGH:
          # apply pts_delta
          if pts_delta != pts_utils.kPtsInvalid:
            l = adjust_pts_delta(l, pts_delta)
          must_forward = True
        elif state == STATE_BUFFER_OUT or state == STATE_POST_OUT:
          must_forward = False
          # optimization: punt right away
          if pid in audiostr_pid_d.keys():
            break

      if must_forward:
        proc_out.stdin.write(l + '\n')

    # close the input_file command
    proc_in.communicate()
    # store a valid out pts value
    if pts2 != pts_utils.kPtsInvalid:
      pts0 = pts2
    else:
      pts0 = farthest_video_pts

  # close the output command
  proc_out.communicate()


def main(argv):
  vals = get_opts(argv)
  # print results
  if vals.debug > 1:
    for k, v in vars(vals).iteritems():
      print 'vals.%s = %s' % (k, v)
    print 'remaining: %r' % vals.remaining

  # check input file specs
  for input_file_spec in vals.input_file_spec:
    res, _, _, _ = parse_input_file_spec(input_file_spec)
    if res < 0:
      print 'error: invalid input file spec: "%s"' % input_file_spec
      sys.exit(-1)

  do_print = (vals.debug >= 0)
  df = splice_streams(vals.input_file_spec, vals.output_filename,
      vals.simple, vals.debug, frames_to_pts(vals.splice_frames))


if __name__ == '__main__':
  main(sys.argv)
