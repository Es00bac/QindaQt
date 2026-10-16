// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <chrono>
namespace QindaQt::LockGreeter {
// Pure bounded exponential backoff: 250 ms, 500 ms, 1 s ... capped at 30 s, and
// never giving up. No clock and no timer: the owner schedules each returned
// delay, which keeps retry rules deterministic to test.
class RetryBackoff {
public:
  constexpr explicit RetryBackoff(std::chrono::milliseconds initial = std::chrono::milliseconds{250},
                                  std::chrono::milliseconds maximum = std::chrono::seconds{30})
      : m_initial(std::min(initial, maximum)), m_maximum(maximum), m_next(m_initial) {}
  // Delay before the next retry; the one after it doubles, up to the maximum.
  constexpr std::chrono::milliseconds next() {
    const auto delay = m_next;
    m_next = std::min(m_maximum, m_next * 2);
    ++m_failures;
    return delay;
  }
  constexpr void reset() {
    m_next = m_initial;
    m_failures = 0;
  }
  constexpr unsigned failures() const { return m_failures; }

private:
  std::chrono::milliseconds m_initial, m_maximum, m_next;
  unsigned m_failures = 0;
};
} // namespace QindaQt::LockGreeter
