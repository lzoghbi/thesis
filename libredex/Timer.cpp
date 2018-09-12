/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "Timer.h"

#include "Trace.h"

unsigned Timer::s_indent = 0;
std::mutex Timer::s_lock;
Timer::times_t Timer::s_times;

Timer::Timer(const std::string& msg)
  : m_msg(msg),
    m_start(std::chrono::high_resolution_clock::now())
{
  ++s_indent;
}

Timer::~Timer() {
  --s_indent;
  auto end = std::chrono::high_resolution_clock::now();
  auto duration_s = std::chrono::duration<double>(end - m_start).count();
  TRACE(TIME, 1, "%*s%s completed in %.1lf seconds\n",
        4 * s_indent, "",
        m_msg.c_str(),
        duration_s);

  {
    std::lock_guard<std::mutex> guard(s_lock);
    s_times.push_back({std::move(m_msg), duration_s});
  }
}
