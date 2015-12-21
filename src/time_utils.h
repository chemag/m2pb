// Copyright Google Inc. Apache 2.0.

#ifndef TIME_UTILS_H_
#define TIME_UTILS_H_

#include <stdint.h>  // for int64_t, uint8_t, uint16_t, etc
#include <sys/time.h>


const int64_t kMsecsPerSec = 1000LL;
const int64_t kUsecsPerSec = 1000000LL;
const int64_t kUsecsPerMsec = 1000LL;
const int64_t kNsecsPerSec = 1000000000LL;
const int64_t kNsecsPerUsec = 1000LL;
const int64_t kNsecsPerMsec = 1000000LL;
const int64_t kOneDayInSec = 24 * 60 * 60;

const int64_t kUnixTimeInvalid = -1;

static inline int64_t secs_to_msecs(int64_t secs) {
  return secs * kMsecsPerSec;
}

static inline int64_t msecs_to_secs(int64_t msecs) {
  return msecs / kMsecsPerSec;
}

static inline int64_t secs_to_usecs(int64_t secs) {
  return secs * kUsecsPerSec;
}

static inline int64_t usecs_to_secs(int64_t usecs) {
  return usecs / kUsecsPerSec;
}

static inline int64_t msecs_to_usecs(int64_t msecs) {
  return msecs * kUsecsPerMsec;
}

static inline int64_t usecs_to_msecs(int64_t usecs) {
  return usecs / kUsecsPerMsec;
}

static inline int64_t usecs_to_nsecs(int64_t usecs) {
  return usecs * kNsecsPerUsec;
}

static inline int64_t nsecs_to_usecs(int64_t nsecs) {
  return nsecs / kNsecsPerUsec;
}

static inline int64_t timeval_to_usecs(const struct timeval *tv) {
  return (secs_to_usecs(tv->tv_sec) + tv->tv_usec);
}

static inline struct timeval usecs_to_timeval(int64_t usecs) {
  struct timeval out;
  out.tv_sec = usecs / kUsecsPerSec;
  out.tv_usec = (usecs % kUsecsPerSec);
  return out;
}

static inline int64_t get_unix_time_usec() {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) < 0)
    return -1;
  return timeval_to_usecs(&tv);
}

static inline void add_usecs_to_timespec(struct timespec *tv,
                                         int64_t to_add_usec) {
  if (to_add_usec <= 0) return;

  tv->tv_sec += to_add_usec / kUsecsPerSec;
  tv->tv_nsec += usecs_to_nsecs(to_add_usec % kUsecsPerSec);
  if (tv->tv_nsec >= kNsecsPerSec) {
    tv->tv_nsec -= kNsecsPerSec;
    tv->tv_sec++;
  }
}

static inline void timeval_normalize(struct timeval *tv) {
  if (tv->tv_usec < kUsecsPerSec) return;
  tv->tv_sec += tv->tv_usec / kUsecsPerSec;
  tv->tv_usec = tv->tv_usec % kUsecsPerSec;
}

#endif  // TIME_UTILS_H_
