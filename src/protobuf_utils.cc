// Copyright Google Inc. Apache 2.0.

#include "protobuf_utils.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <stdio.h>   // for NULL, fprintf, stderr
#include <stdlib.h>  // for exit

#include <fstream>  // for basic_ostream, etc
#include <sstream>  // for ostringstream
#include <string>   // for string, operator<<, etc

using namespace google::protobuf;

bool field_exists(const Descriptor* descriptor, const std::string& name) {
  std::string s(name);
  char delimiter = '.';
  size_t pos = 0;
  const FieldDescriptor* fd = NULL;
  while ((pos = s.find(delimiter)) != std::string::npos) {
    // get the field name
    std::string field_name = s.substr(0, pos);
    s.erase(0, pos + 1);
    // get the field
    fd = descriptor->FindFieldByName(field_name);
    if (fd == NULL) {
      return false;
    }
    descriptor = fd->message_type();
  }
  // dereference the last item
  return descriptor->FindFieldByName(s) != NULL;
}

static bool get_field(const Message& message, const std::string& name,
                      const FieldDescriptor** out_fd, const Message** out_m) {
  std::string s(name);
  char delimiter = '.';
  size_t pos = 0;
  const Message* m = &message;
  const Descriptor* d = NULL;
  const FieldDescriptor* fd = NULL;
  const Reflection* reflection = NULL;
  while ((pos = s.find(delimiter)) != std::string::npos) {
    // get the field name
    std::string field_name = s.substr(0, pos);
    s.erase(0, pos + 1);
    // get the field
    d = m->GetDescriptor();
    fd = d->FindFieldByName(field_name);
    if (!fd) {
      return false;
    }
    // get the field message
    reflection = m->GetReflection();
    m = &reflection->GetMessage(*m, fd);
  }
  // dereference the last item
  // get the field descriptor
  d = m->GetDescriptor();
  fd = d->FindFieldByName(s);
  if (!fd) {
    return false;
  }
  *out_fd = fd;
  *out_m = m;
  return true;
}

bool get_field_value(const Message& msg, const std::string& name,
                     std::string* value) {
  const FieldDescriptor* fd;
  const Message* m;
  if (!get_field(msg, name, &fd, &m)) {
    // field <name> does not exist at all
    return false;
  }
  const Reflection* reflection = m->GetReflection();
  if (!reflection->HasField(*m, fd)) {
    // msg has no field <name>
    return false;
  }
  // print the value
  std::ostringstream ss;
  switch (fd->type()) {
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      ss << reflection->GetInt32(*m, fd);
      break;
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      ss << reflection->GetInt64(*m, fd);
      break;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      ss << reflection->GetUInt32(*m, fd);
      break;
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      ss << reflection->GetUInt64(*m, fd);
      break;
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      ss << reflection->GetFloat(*m, fd);
      break;
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      ss << reflection->GetDouble(*m, fd);
      break;
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      ss << reflection->GetBool(*m, fd);
      break;
    case google::protobuf::FieldDescriptor::TYPE_STRING:
      ss << '"' << reflection->GetString(*m, fd) << '"';
      break;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      ss << "{ " << reflection->GetMessage(*m, fd).ShortDebugString() << " }";
      break;
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      ss << reflection->GetEnum(*m, fd);
      break;
    default:
      // TODO(chema): support this
      fprintf(stderr, "ERROR: Unsupported pb type: %i\n", fd->type());
      exit(-1);
      break;
  }
  *value = ss.str();
  return true;
}
