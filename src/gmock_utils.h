#ifndef SRC_GMOCK_UTILS_H_
#define SRC_GMOCK_UTILS_H_

#include <gmock/gmock.h>

// Matcher that checks against the given expected proto.
template <typename T>
::testing::Matcher<const T &> EqualsProto(const T &expected) {
  return ::testing::Property(&T::ShortDebugString, expected.ShortDebugString());
}

#endif  // SRC_GMOCK_UTILS_H_
