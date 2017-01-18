// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

/*
 * THIS FILE WILL LIKELY BE INTEGRATED IN Throttle.h. HOWEVER, SINCE
 * Throttle.h IS WIDELY INCLUDED, ANY CHANGES CAUSE LOTS OF
 * RECOMPILATION. SO FOR THE TIME BEING, WE'RE SEPARATING THIS OUT TO
 * REDUCE COMPILATION TIMES.
 */

#pragma once

#include <ostream>
#include "Mutex.h"
#include "common/Clock.h"

// this will clobber _ASSERT_H
#include "spline.h"
// the following is done to unclobber _ASSERT_H so it returns to the
// way ceph likes it
#include "include/assert.h"


class CubicModelThrottle : public BackoffThrottle {
  ::tk::spline spline;

public:
  CubicModelThrottle(
    unsigned expected_concurrency ///< [in] determines size of conds
    ) :
    BackoffThrottle(expected_concurrency)
  { }

}; // class CubicModelThrottle


template<typename T>
class DataCollectionThrottle {
  struct ThrottleTiming {
    utime_t start_time;
    uint32_t tx_count;
    uint64_t tx_size;
    uint64_t tx_total_size;

    ThrottleTiming(uint32_t tx_count, uint64_t tx_size, uint64_t tx_total_size) :
      start_time(ceph_clock_now(NULL)),
      tx_count(tx_count),
      tx_size(tx_size),
      tx_total_size(tx_total_size)
    { }
  };

  std::string name;
  std::map<T,ThrottleTiming> map;
  uint32_t count;
  uint64_t total;
  Mutex lock;

public:

  DataCollectionThrottle(const std::string& name) :
    name(name),
    count(0),
    total(0),
    lock(name)
  { }

  DataCollectionThrottle(const char* name) :
    name(name),
    count(0),
    total(0),
    lock(name)
  { }

  uint64_t get_current() {
    return total;
  }

  void get(uint32_t tx_size, T index) {
    Mutex::Locker l(lock);
    total += tx_size;
    ++count;
    map.emplace(index, ThrottleTiming(count, tx_size, total));
  }

  ostream& put(T index, ostream& out) {
    utime_t duration;
    uint64_t count_at_get;
    uint64_t total_at_get;
    {
      Mutex::Locker l(lock);
      auto it = map.find(index);
      assert(it != map.end());
      --count;
      count_at_get = it->second.tx_count;
      total -= it->second.tx_size;
      total_at_get = it->second.tx_total_size;
      duration = ceph_clock_now(NULL) - it->second.start_time;
      map.erase(it);
    }
    out << name << ": (" << count_at_get << "," << total_at_get <<
      "," << duration.to_nsec() << ")";
    return out;
  }
}; // class DataCollectionThrottle
