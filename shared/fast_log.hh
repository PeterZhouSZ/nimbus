/*
 * Copyright 2013 Stanford University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the
 *   distribution.
 *
 * - Neither the name of the copyright holders nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 /*
  * Nimbus log interface.
  *
  * Author: Hang Qu <quhang@stanford.edu>
  */

#ifndef NIMBUS_SHARED_FAST_LOG_H_
#define NIMBUS_SHARED_FAST_LOG_H_

#include <pthread.h>
#include <sys/types.h>

#include <cassert>
#include <cstdio>
#include <map>
#include <utility>

namespace nimbus {
namespace timer {

enum TimerType {
  kTotal = 0,
  kExecuteComputationJob,
  kExecuteCopyJob,
  kAssemblingCache,
  kMaxCounter
};
struct TimerRecord {
  TimerRecord() : depth(0), sum(0) {
  }
  struct timespec old_timestamp;
  struct timespec new_timestamp;
  int64_t depth;
  int64_t sum;
};

typedef std::map<std::pair<pid_t, TimerType>, TimerRecord*> TimersMap;
extern TimersMap timers_map;
extern pthread_key_t keys[kMaxCounter];

void InitializeKeys();
void InitializeTimers();
void PrintTimerSummary(FILE* output = stdout);

inline void StartTimer(TimerType timer_type) {
  void* ptr = pthread_getspecific(keys[timer_type]);
  TimerRecord* record = static_cast<TimerRecord*>(ptr);
  assert(record);
  clock_gettime(CLOCK_REALTIME, &(record->new_timestamp));
  if (record->depth != 0) {
    record->sum +=
        record->depth * (
             (record->new_timestamp.tv_sec - record->old_timestamp.tv_sec) * 1e9
             + record->new_timestamp.tv_nsec - record->old_timestamp.tv_nsec);
  }
  record->old_timestamp = record->new_timestamp;
  ++record->depth;
}

inline void StopTimer(TimerType timer_type) {
  void* ptr = pthread_getspecific(keys[timer_type]);
  TimerRecord* record = static_cast<TimerRecord*>(ptr);
  assert(record);
  clock_gettime(CLOCK_REALTIME, &(record->new_timestamp));
  record->sum +=
      record->depth * (
           (record->new_timestamp.tv_sec - record->old_timestamp.tv_sec) * 1e9
           + record->new_timestamp.tv_nsec - record->old_timestamp.tv_nsec);
  record->old_timestamp = record->new_timestamp;
  --record->depth;
  assert(record->depth >= 0);
}

}  // namespace timer
}  // namespace nimbus


#endif  // NIMBUS_SHARED_FAST_LOG_H_
