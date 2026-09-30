// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "attempt_coordinator.h"
#include <chrono>
#include <optional>
#include <string>
namespace QindaQt::LockAuthentication {
enum class WireKind : unsigned char {
  Begin = 1, Secret, Visible, Information, Error, Response, Cancel, Result
};
struct WireFrame {
  WireKind kind;
  AttemptToken token;
  std::string payload;
};
// AGENT-CONTRACT: native controller and disposable PAM worker share this bounded
// private inherited socket format, not D-Bus or QML. Tokens scope messages; they
// are not authentication credentials. One channel/absolute deadline per attempt.
// Constructor takes FD ownership even on failure; destructor closes it. Blocking
// calls belong only to the worker/test thread; GUI uses asynchronous framing.
class WorkerChannel final {
public:
  explicit WorkerChannel(int ownedFd, std::chrono::milliseconds timeout);
  ~WorkerChannel();
  WorkerChannel(const WorkerChannel &) = delete;
  WorkerChannel &operator=(const WorkerChannel &) = delete;
  bool send(const WireFrame &frame);
  std::optional<WireFrame> receive();
  bool pending() const;
  bool failed() const { return m_failed; }
  static std::optional<std::string> encode(const WireFrame &frame);
  // Returns nullopt for malformed OR incomplete input; caller checks exact size
  // before accepting. max frame length is fixed so no unbounded buffering.
  static std::optional<WireFrame> decode(std::string_view bytes);
  static constexpr std::size_t headerBytes = 24;
  static constexpr std::size_t maximumFrameBytes = headerBytes + maximumPromptBytes;
private:
  bool transfer(char *bytes, std::size_t size, bool writing);
  int m_fd;
  std::chrono::steady_clock::time_point m_deadline;
  bool m_failed = false;
};
} // namespace QindaQt::LockAuthentication
