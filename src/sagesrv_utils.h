// Copyright Google Inc. Apache 2.0.

#ifndef SAGESRV_UTILS_H_
#define SAGESRV_UTILS_H_

#include <inttypes.h>  // for PRId64


// Tries to interpret a chunk as a sagesrv message. It returns 0 if it can,
// -1 otherwise. If the message is MEDIACMD_DVD_NEWCELL_NOREPLY (used to
// send pts delta values to miniclient), it will set pts_delta.
int sagesrv_interpret_buffer(uint8_t *buffer, int len, int debug,
    int64_t *pts_delta);

#endif  // SAGESRV_UTILS_H_
