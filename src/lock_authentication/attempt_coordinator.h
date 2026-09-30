// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "authentication.h"
#include <cstdint>
#include <optional>
namespace QindaQt::LockAuthentication {
struct AttemptToken {
  std::uint64_t epoch = 0;
  std::uint64_t request = 0;
  friend bool operator==(const AttemptToken &, const AttemptToken &) = default;
};
enum class Completion { Ignored, Retry, Unlock };
// GUI-thread coordinator. The trusted native protocol adapter alone enters or
// leaves a lock epoch; QML can request an attempt or cancel, never complete
// one. Completion comes only from the owned native worker's private response
// pipe. No secrets are stored here. A cancelled/stale response cannot unlock or
// consume a newer attempt, and successful approval is consumed exactly once.
class AttemptCoordinator {
public:
  void enterLockedSession();
  void leaveLockedSession();
  std::optional<AttemptToken> begin();
  void cancel();
  Completion complete(AttemptToken token, Outcome outcome);
  bool busy() const { return m_pending.has_value(); }

private:
  std::uint64_t m_epoch = 0;
  std::uint64_t m_request = 0;
  bool m_locked = false;
  std::optional<AttemptToken> m_pending;
};
} // namespace QindaQt::LockAuthentication
