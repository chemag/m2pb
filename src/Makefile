# Copyright Google Inc. Apache 2.0.

all: .protos_done m2pb

CXX = g++
CFLAGS = -g -O0 -Wall -pedantic -std=c++0x
LDFLAGS=mpeg2ts_parser.o mpeg2ts_reader.o \
		mpeg2ts.pb.o protobuf_utils.o ac3_utils.o h264_utils.o bitstream.o
LIBS+=-lprotobuf -lpthread

m2pb: m2pb.cc mpeg2ts_parser.o mpeg2ts_reader.o mpeg2ts.pb.o \
    protobuf_utils.o ac3_utils.o h264_utils.o bitstream.o
	$(CXX) $(CFLAGS) -c m2pb.cc -o m2pb.o
	$(CXX) $(CFLAGS) -o m2pb m2pb.o $(LDFLAGS) $(LIBS)

bitstream.o: bitstream.cc bitstream.h
	$(CXX) $(CFLAGS) -c bitstream.cc -o bitstream.o

mpeg2ts_parser.o: mpeg2ts_parser.cc mpeg2ts_parser.h mpeg2ts.pb.h
	$(CXX) $(CFLAGS) -c mpeg2ts_parser.cc -o mpeg2ts_parser.o

mpeg2ts_reader.o: mpeg2ts_reader.cc mpeg2ts_reader.h
	$(CXX) $(CFLAGS) -c mpeg2ts_reader.cc -o mpeg2ts_reader.o

protobuf_utils.o: protobuf_utils.cc protobuf_utils.h
	$(CXX) $(CFLAGS) -c protobuf_utils.cc -o protobuf_utils.o

ac3_utils.o: ac3_utils.cc ac3_utils.h
	$(CXX) $(CFLAGS) -c ac3_utils.cc -o ac3_utils.o

h264_utils.o: h264_utils.cc h264_utils.h
	$(CXX) $(CFLAGS) -c h264_utils.cc -o h264_utils.o

.protos_done: mpeg2ts.proto
	$(MAKE) mpeg2ts.pb.h
	$(MAKE) mpeg2ts_pb2.py

PROTOC = protoc
PROTOFLAGS=--cpp_out=. -I.

mpeg2ts.pb.o: mpeg2ts.pb.h
	$(CXX) $(CFLAGS) -c mpeg2ts.pb.cc -o mpeg2ts.pb.o

mpeg2ts.pb.h: mpeg2ts.proto
	echo "Building mpeg2ts.pb.h"
	$(PROTOC) $(PROTOFLAGS) $<

mpeg2ts_pb2.py: mpeg2ts.proto
	echo "Building mpeg2ts_pb2.py"
	$(PROTOC) -I=. --python_out=. $<

mpeg2ts_parser_test: mpeg2ts_parser_test.cc
	$(CXX) $(CFLAGS) -c mpeg2ts_parser_test.cc -o mpeg2ts_parser_test.o
	$(CXX) $(CFLAGS) -o mpeg2ts_parser_test mpeg2ts_parser_test.o $(LDFLAGS) $(LIBS) -lgmock

test: mpeg2ts_parser_test
	./mpeg2ts_parser_test

clean:
	rm -f m2pb.o m2pb mpeg2ts_parser_test
	rm -rf *.pb.* .protos_done *.o *.pyc mpeg2ts_pb2.py

