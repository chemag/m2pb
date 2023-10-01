m2pb: an mpeg2ts parsing tool
-----------------------------

Copyright 2015 Google Inc.

This is not an official Google product.


# 1. Introduction

m2pb is a tool that parses mpeg2ts (mpeg-ts aka MPEG-2 Part 1, Systems
aka ISO/IEC 13818-1) media streams. It has 2 main uses:

* field cherry-pick
* stream packet-based edition


# 2. Using m2pb for Field Cherry-Picking

The first use case for m2pb is printing the various fields of every
packet in an mpeg-ts field. This is directly inspired by
[ipsumdump](http://www.read.seas.harvard.edu/~kohler/ipsumdump/).
m2pb allows selecting any of the fields in an mpeg-ts stream, and will
print one line per packet. For example:


```
$ m2pb --packet --byte --pid --pts --parsed.pes_packet.dts --type dump -i ../bin/in.ts
packet,byte,parsed.header.pid,parsed.pes_packet.pts,parsed.pes_packet.dts,type
0,0,0,,,?
1,188,256,,,?
2,376,257,901502,900000,X
3,564,257,,,X
4,752,257,,,X
5,940,257,,,X
6,1128,257,,,X
7,1316,257,,,X
8,1504,257,,,X
9,1692,257,,,X
10,1880,257,,,X
11,2068,257,,,X
12,2256,257,,,X
13,2444,257,,,X
14,2632,257,,,X
15,2820,257,,,X
16,3008,257,,,X
17,3196,257,,,X
18,3384,257,,,X
19,3572,257,,,X
20,3760,257,,,X
...
195,36660,257,910511,906006,X
196,36848,257,,,X
197,37036,257,,,X
198,37224,257,,,X
199,37412,257,,,X
```

Fields can be obtained from 3 places:

* direct protobuf field name. In our example, we are using
  the flag "`--parsed.pes_packet.dts`",
* protobuf shortcut: the "`--pts`" flag is a shortcut for
  "`--parsed.pes_packet.pts`",
* function-associated: the "`--type`" and "`--syncframe`" flags have already
  been associated to a given function. "type" prints the h.264 video frame
  type (I, P, or B) and the audio stream number. "syncframe" prints, for
  audio frames, the distance to the first ac-3 syncframe from the
  beginning of the payload.


# 3. Using m2pb for Stream Packet-Based Edition

The second use case is allowing manual or script-based edition of
mpeg-ts packet fields. m2pb provides a mechanism for binary-to-text
and text-to-binary lossless conversion of mpeg2-ts packets. In order to edit
the packets of an mpeg-ts stream, the stream is converted to text,
edit there, and then converted back to binary before writing it back.
The text format is the short text protobuf one.

m2pb can convert an mpeg-ts stream to text:

```
$ ./m2pb --proc totxt -i ../bin/in.ts
packet: 0 byte: 0 parsed { header { transport_error_indicator: false
  payload_unit_start_indicator: true transport_priority: false pid: 0
  transport_scrambling_control: 0 adaptation_field_exists: false
  payload_exists: true continuity_counter: 0 } psi_packet {
  pointer_field: "" program_association_section { table_id: 0
  section_length: 13 transport_stream_id: 1 version_number: 0
  current_next_indicator: true section_number: 0
  last_section_number: 0 program_information { program_number: 1
  program_map_pid: 256 } crc_32: -386310531 } } data_bytes:
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377
   \377\377\377\377\377\377\377\377\377\377\377\377\377\377\377
   \377\377\377\377\377\377\377\377\377\377\377\377\377\377\377
   \377\377\377\377\377\377\377\377\377\377\377\377\377\377\377
   \377\377\377\377\377\377\377\377\377\377\377\377\377\377\377
   \377\377\377\377\377\377\377\377\377\377\377\377\377\377\377
   \377\377\377\377\377\377\377\377\377\377\377\377\377\377\377
   \377\377\377\377\377\377\377\377\377\377\377\377\377\377\377
   \377\377\377\377\377\377\377\377\377\377\377\377\377\377\377
   \377\377\377\377\377\377\377\377\377\377\377\377\377\377\377
   \377\377\377\377\377\377\377\377\377\377\377\377\377\377\377
   \377\377" }
packet: 1 byte: 188 parsed { header { transport_error_indicator: false
  ...
...
```

Note that we are breaking the lines to make the text readable, but
m2pb totxt always produces one line per packet.


m2pb can convert an mpeg-ts text stream back into a binary one:

```
$ diff ../bin/in.ts <(./m2pb --proc totxt -i ../bin/in.ts | ./m2pb --proc tobin)
$ echo $?
0
```

## 3.1. Using m2pb for Manual Stream Packet-Based Edition

For manual packet-based edition, we include a ts.vim script that
associates files with the .ts extension to be pre- and post-parsed
by m2pb. This allows direct edition of mpeg-ts traces using vim.


## 3.2. Using m2pb for Automatic Stream Packet-Based Edition

m2pb allows performing per-packet edition of .ts frames by piping
text-based edition in the middle of a binary-to-text and a
text-to-binary conversion. For example, in order to change the pid of
a given stream (turn the stream with pid 482 to 582), we could run
the following command:

```
$ ffmpeg -i /tmp/in.ts
...
    Stream #0:1[0x1e2](und): Audio: ac3 (AC-3 / 0x332D4341), 48000 Hz,
        stereo, fltp, 192 kb/s
$ m2pb --proc totxt -i ../bin/in.ts | \
  sed -e 's/ pid: 482/ pid:582/' | \
  sed -e 's/elementary_pid: 482/elementary_pid: 582/' | \
  m2pb --proc tobin -i - -o /tmp/out.ts
$ ffmpeg -i /tmp/out.ts
...
    Stream #0:1[0x246](und): Audio: ac3 (AC-3 / 0x332D4341), 48000 Hz,
        stereo, fltp, 192 kb/s
```

# 4. Implementation

At its core, m2pb is an mpeg-ts binary to text converter. It converts
mpeg-ts files (binary) to a text representation, and from the text
representation back to the binary mpeg-ts file. The process is lossless.

In order to make the implementation of the text back-and-forth conversion
easy (both generating -- dumping -- and parsing a text version of
mpeg-ts), we rely on an intermediate step where we use protocol buffers
(aka protobuf).

[Protocol buffers](https://developers.google.com/protocol-buffers/) is
a serialization mechanism with structured data. Sort of json with
types. Protobuf also has a handy text mode, which allows succint,
one-line text representation of a protobuf object. It also provides
straightforward protobuf-to-text and text-to-protobuf conversion
mechanisms. m2pb defines a protobuf structure to represent mpeg-ts
packets, and protobuf auto-generates the text-to-protobuf parser
and dumper.

Therefore, m2pb is left with the task to implement 2 converters,
namely the "parser" (the mpeg-ts binary to protobuf converter) and
the "dumper" (the protobuf to mpeg-ts binary converter).


    +----------+ Mpeg2TsParser::Parse* +---------------+ protobuf +-----------+
    |bin mpegts|---------------------->|protobuf mpegts|<-------->|text mpegts|
    +----------+<--------------------- +---------------+          +-----------+
                Mpeg2TsParser::Dump*


## 4.1. Other Features

m2pb supports mpeg-ts resync (it will dump any chunk in the input stream
that is not an mpeg-ts packet as a non-parsed packet).



# 5. Installation

## 5.1. Install Preparation

The main dependency is protobuf.

On Ubuntu, use:
```
$ sudo apt-get install protobuf-compiler libprotobuf-dev googletest googletest-tools
```

On Fedora, use:
```
$ sudo dnf install protobuf-c-compiler protobuf-c-devel gtest-devel
```


## 5.2. Install m2pb

```
$ cd src
$ make -j
```
