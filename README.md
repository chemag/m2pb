m2pb: an mpeg2ts parsing tool
-----------------------------

Copyright 2015 Google Inc.

This is not an official Google product.


# Introduction

m2pb is a tool that parses mpeg2ts (mpeg-ts aka MPEG-2 Part 1, Systems
aka ISO/IEC 13818-1) media streams. It has 2 main uses:

* field cherry-pick
* stream packet-based edition


# Using m2pb for Field Cherry-Picking

The first use case for m2pb is printing the various fields of every
packet in an mpeg-ts field. This is directly inspired by
[ipsumdump](http://www.read.seas.harvard.edu/~kohler/ipsumdump/).
m2pb allows selecting any of the fields in an mpeg-ts stream, and will
print one line per packet. For example:


```
$ m2pb --packet --byte --pid --pts --parsed.pes_packet.dts --type dump in.ts
0 0 0 - - -
1 188 480 - - -
2 376 481 183003 180000 I
3 564 481 - - -
4 752 481 - - -
5 940 481 - - -
6 1128 481 - - -
7 1316 481 - - -
8 1504 481 - - -
9 1692 481 - - -
10 1880 481 - - -
11 2068 481 - - -
12 2256 481 - - -
13 2444 481 - - -
14 2632 481 - - -
15 2820 481 - - -
16 3008 481 - - -
17 3196 481 198018 183003 P
18 3384 481 186006 - B
19 3572 481 189009 - B
20 3760 481 192012 - B
21 3948 481 195015 - B
22 4136 481 213033 198018 P
...
8116 1525808 482 183003 - 1
```

Fields can be obtained from 3 places:

* direct protobuf field name. In our example, we are using
  the flag "--parsed.pes_packet.dts",
* protobuf shortcut: the "--pts" flag is a shortcut for
  "--parsed.pes_packet.pts",
* function-associated: the "--type" and "--syncframe" flags have already
  been associated to a given function. "type" prints the h.264 video frame
  type (I, P, or B) and the audio stream number. "syncframe" prints, for
  audio frames, the distance to the first ac-3 syncframe from the
  beginning of the payload.


# Using m2pb for Stream Packet-Based Edition

The second use case is allowing manual or script-based edition of
mpeg-ts packet fields. m2pb provides a mechanism for binary-to-text
and text-to-binary conversion of mpeg2-ts packets. In order to edit
the packets of an mpeg-ts stream, the stream is converted to text,
edit there, and then converted back to binary before writing it back.
The text format is the short text protobuf one.

m2pb can convert an mpeg-ts stream to text:

```
$ m2pb totxt in.ts
packet: 0 byte: 0 parsed { header { transport_error_indicator: false
  payload_unit_start_indicator: false transport_priority: false pid: 481
  transport_scrambling_control: 2 adaptation_field_exists: false
  payload_exists: true continuity_counter: 15 } data_bytes: "\320\352
  \001\220\355\311H \325@\225\177\340I\006\005\342\017`\300\221;I~\366
  \247\370\364\201\222y\310\372\302\275.o\362\254\324,x;\376u\313\'
  \365\375\0025\013\0068\364_m\2767p\301\024\'\367a\312$\257fK\244
  \354\235\275=\300\003p\036\335\233P\334\341Ao\246\223o\215\367\370
  #\002\322s\370\023b\300\251P[T\235\240\036\022\332S\023\333+{\310
  {Q\344\255\345\237W\326\3502\231\262V\003\211]\326K \200nO\306\270
  \325\301\262\220\340d\307\006\272\235P\254f\\\3353\242\257r\217\202
  \035\214\342\203\344\3111\225K\200r\014\317\376\037l\t 3\215\335I\336\327" }
packet: 1 byte: 188 parsed { header { transport_error_indicator: false
  payload_unit_start_indicator: false transport_priority: false pid: 481
  transport_scrambling_control: 2 adaptation_field_exists: false
  payload_exists: true continuity_counter: 0 } data_bytes: "\202\305i
  \355\207\373/m6X\207^\036\2336\2260U^[\'3\330\267\340\214\342>\340\206
  \317\207\221v7\264\3459\350PS\265\212\372\376\247^3\246l5\306E\035a
  \234\326tWz|\307X\307U\271\222H1g9\031\"\302C\305\255\237,\254.4f\240
  \266IX0\207\220z\203\223\026\"}\311\324\307\033\206\373\363\233\224
  \306P\207\257\365\354\362\013\271O\351\022\231\240\340t\3145#\206\024
  Pk\275?\346\337\337\305\010\370S\020<D\004\275\332\201\003\351\377\217
  KY\272A\341\367\245\262\325I\365\346q\274w\306\274\255\234\372j\272d
  \235\\\262\t<\335\263F\207\003\232\376\302" }
...
```

Note that we are breaking the lines to make the text readable, but
m2pb totxt always produces one line per packet.


m2pb can convert an mpeg-ts text stream back into a binary one:

```
$ diff in.ts <(m2pb totxt in.ts | m2pb tobin -)
$ echo $?
0
```

## Using m2pb for Manual Stream Packet-Based Edition

For manual packet-based edition, we include a ts.vim script that
associates files with the .ts extension to be pre- and post-parsed
by m2pb. This allows direct edition of mpeg-ts traces using vim.


## Using m2pb for Automatic Stream Packet-Based Edition

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
$ m2pb totxt /tmp/in.ts | \
  sed -e 's/ pid: 482/ pid:582/' | \
  sed -e 's/elementary_pid: 482/elementary_pid: 582/' | \
  m2pb tobin - /tmp/out.ts
$ ffmpeg -i /tmp/out.ts
...
    Stream #0:1[0x246](und): Audio: ac3 (AC-3 / 0x332D4341), 48000 Hz,
        stereo, fltp, 192 kb/s
```

# Implementation

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


## Other Features

m2pb supports mpeg-ts resync (it will dump any chunk in the input stream
that is not an mpeg-ts packet as a non-parsed packet).

