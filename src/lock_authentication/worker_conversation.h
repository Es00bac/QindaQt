// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "worker_wire.h"
namespace QindaQt::LockAuthentication {
// Borrowed worker-only channel. Any unexpected input/EOF is cancellation;
// the controller must invalidate its token immediately before sending Cancel.
class WorkerConversation final : public Conversation {
public:
  WorkerConversation(WorkerChannel &channel, AttemptToken token);
  std::optional<std::string> exchange(MessageKind kind, std::string_view prompt) override;
  bool cancelled() const override;
private:
  WorkerChannel &m_channel;
  AttemptToken m_token;
  mutable bool m_cancelled = false;
};
} // namespace QindaQt::LockAuthentication
