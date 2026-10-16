// SPDX-License-Identifier: GPL-3.0-or-later
#include "output_window_supervisor.h"
#include <algorithm>
namespace QindaQt::LockGreeter {
void OutputWindowSupervisor::outputAdded(OutputKey output) {
  const auto known = m_outputs.find(output);
  if (known != m_outputs.end() && (known->second.window || known->second.retry)) return;
  attempt(output);
}
void OutputWindowSupervisor::outputRemoved(OutputKey output) {
  const auto known = m_outputs.find(output);
  if (known == m_outputs.end()) return;
  const bool window = known->second.window, retry = known->second.retry;
  m_outputs.erase(known);
  if (retry) m_scheduler.cancel(output);
  if (window) m_factory.destroy(output);
}
void OutputWindowSupervisor::retryDue(OutputKey output) {
  const auto known = m_outputs.find(output);
  if (known == m_outputs.end() || known->second.window || !known->second.retry) return;
  attempt(output);
}
void OutputWindowSupervisor::attempt(OutputKey output) {
  m_outputs[output].retry = false;
  // AGENT-GUARD: no reference into m_outputs may live across create(): window
  // creation runs Qt and Wayland code that can deliver screenAdded/screenRemoved
  // reentrantly, which would invalidate it.
  const auto result = m_factory.create(output);
  const auto known = m_outputs.find(output);
  if (known == m_outputs.end()) {
    // Removed while being created: its window must not outlive its output.
    if (result == OutputWindowFactory::Result::Created) m_factory.destroy(output);
    return;
  }
  switch (result) {
  case OutputWindowFactory::Result::Created:
    known->second.window = true;
    known->second.backoff.reset();
    return;
  case OutputWindowFactory::Result::NotAnOutput:
    m_outputs.erase(known);
    return;
  case OutputWindowFactory::Result::Failed:
    known->second.retry = true;
    m_scheduler.schedule(output, known->second.backoff.next());
    return;
  }
}
bool OutputWindowSupervisor::hasWindow(OutputKey output) const {
  const auto known = m_outputs.find(output);
  return known != m_outputs.end() && known->second.window;
}
bool OutputWindowSupervisor::retryPending(OutputKey output) const {
  const auto known = m_outputs.find(output);
  return known != m_outputs.end() && known->second.retry;
}
unsigned OutputWindowSupervisor::failures(OutputKey output) const {
  const auto known = m_outputs.find(output);
  return known == m_outputs.end() ? 0U : known->second.backoff.failures();
}
std::size_t OutputWindowSupervisor::windowCount() const {
  return static_cast<std::size_t>(std::count_if(m_outputs.begin(), m_outputs.end(),
                                                [](const auto &entry) { return entry.second.window; }));
}
std::size_t OutputWindowSupervisor::pendingRetries() const {
  return static_cast<std::size_t>(std::count_if(m_outputs.begin(), m_outputs.end(),
                                                [](const auto &entry) { return entry.second.retry; }));
}
} // namespace QindaQt::LockGreeter
