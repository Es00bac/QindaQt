// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace QindaQt::LockAuthentication {
enum class MessageKind { Secret, Visible, Information, Error };
enum class Outcome {
  Authenticated,
  Denied,
  AccountDenied,
  Cancelled,
  Unavailable
};

// AGENT-CONTRACT: a conversation belongs to one disposable native PAM worker.
// Methods run synchronously on that worker, never on the QML/GUI thread. The
// transport enforces its deadline and may return nullopt on cancellation/EOF.
// Prompt/response bytes must never enter diagnostic output. Returned responses
// are consumed and wiped immediately; no caller-provided username is accepted.
class Conversation {
public:
  virtual ~Conversation() = default;
  virtual std::optional<std::string> exchange(MessageKind kind,
                                              std::string_view prompt) = 0;
  virtual bool cancelled() const = 0;
};

// This internal port isolates libpam for focused tests. Production constructs
// only the native implementation; the worker protocol offers no injection or
// success request. Implementations own their PAM handle for exactly one
// attempt.
class PamTransaction {
public:
  virtual ~PamTransaction() = default;
  virtual bool start(std::string_view service, std::string_view user,
                     Conversation &conversation) = 0;
  virtual bool authenticate() = 0;
  virtual bool approveAccount() = 0;
};

// Synchronous and fail-closed: authentication AND account approval are
// required. The caller owns both borrowed arguments throughout the attempt.
// Native worker identity is the real UID's account; service is fixed, never
// caller-controlled.
Outcome authenticateSessionUser(PamTransaction &pam, Conversation &conversation,
                                std::string_view sessionUser);
inline constexpr std::size_t maximumPromptBytes = 4096;
inline constexpr std::size_t maximumResponseBytes = 4096;
inline constexpr std::size_t maximumMessages = 64;
} // namespace QindaQt::LockAuthentication
