// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "retry_backoff.h"
#include <chrono>
#include <cstddef>
#include <map>
namespace QindaQt::LockGreeter {
// Opaque identity of one output (the QScreen address in production). Never
// dereferenced here.
using OutputKey = const void *;

// Creates and destroys the lock-role window of one output. A Failed result must
// already have logged its own reason; NotAnOutput means the screen has no
// physical output (Qt's client-only placeholder) or is gone, so there is
// nothing to supervise.
class OutputWindowFactory {
public:
  enum class Result { Created, NotAnOutput, Failed };
  virtual ~OutputWindowFactory() = default;
  virtual Result create(OutputKey output) = 0;
  virtual void destroy(OutputKey output) = 0;
};

// Owner-provided one-shot timers, one pending retry per output.
class RetryScheduler {
public:
  virtual ~RetryScheduler() = default;
  virtual void schedule(OutputKey output, std::chrono::milliseconds delay) = 0;
  virtual void cancel(OutputKey output) = 0;
};

// Decides what happens when an output's window cannot be created. A failure on
// one output never ends the locker and never disturbs the windows of the others:
// the failed output is retried with bounded exponential backoff (250 ms doubling
// to 30 s, unlimited) while the compositor keeps it covered by its own protected
// fallback. Pure policy: no Qt, clock or timer, all injected. GUI thread only.
class OutputWindowSupervisor {
public:
  OutputWindowSupervisor(OutputWindowFactory &factory, RetryScheduler &scheduler)
      : m_factory(factory), m_scheduler(scheduler) {}
  void outputAdded(OutputKey output);
  void outputRemoved(OutputKey output);
  // The scheduler's timer for `output` fired.
  void retryDue(OutputKey output);
  bool hasWindow(OutputKey output) const;
  bool retryPending(OutputKey output) const;
  unsigned failures(OutputKey output) const;
  std::size_t windowCount() const;
  std::size_t pendingRetries() const;

private:
  struct Entry {
    bool window = false;
    bool retry = false;
    RetryBackoff backoff;
  };
  void attempt(OutputKey output);
  OutputWindowFactory &m_factory;
  RetryScheduler &m_scheduler;
  std::map<OutputKey, Entry> m_outputs;
};
} // namespace QindaQt::LockGreeter
