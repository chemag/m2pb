// Copyright Google Inc. Apache 2.0.

#ifndef AC3_UTILS_H_
#define AC3_UTILS_H_

#include <inttypes.h>  // for PRId64

// Returns the distance to the first AC-3 syncframe in a buffer (-1 if
// no syncframe in the buffer).
int ac3_syncframe_distance(const uint8_t *data, int len);

#endif  // AC3_UTILS_H_
