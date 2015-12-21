// Copyright Google Inc. Apache 2.0.

#ifndef PROTOBUF_UTILS_H_
#define PROTOBUF_UTILS_H_

#include <string>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/text_format.h>


// Returns whether a protobuf message class has a given field. It
// supports nested field names (e.g. "field1.field2.field3").
bool field_exists(const google::protobuf::Descriptor* descriptor,
    const std::string &name);

// Get the value of a named field (printed as a string in <value>)
// for a given protobuf message.
// Returns whether it was able to get the field name.
bool get_field_value(const google::protobuf::Message &msg,
    const std::string &name, std::string *value);

#endif  // PROTOBUF_UTILS_H_
