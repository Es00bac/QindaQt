// SPDX-License-Identifier: GPL-3.0-or-later
#include "worker_conversation.h"
#include <cstring>
namespace QindaQt::LockAuthentication {
WorkerConversation::WorkerConversation(WorkerChannel &channel, AttemptToken token)
    : m_channel(channel), m_token(token) {}
std::optional<std::string> WorkerConversation::exchange(MessageKind kind, std::string_view prompt) {
  if (cancelled()) return std::nullopt;
  WireKind wire;
  switch (kind) {
  case MessageKind::Secret: wire = WireKind::Secret; break;
  case MessageKind::Visible: wire = WireKind::Visible; break;
  case MessageKind::Information: wire = WireKind::Information; break;
  case MessageKind::Error: wire = WireKind::Error; break;
  default: m_cancelled = true; return std::nullopt;
  }
  if (!m_channel.send({wire, m_token, std::string(prompt)})) {
    m_cancelled = true; return std::nullopt;
  }
  if (kind == MessageKind::Information || kind == MessageKind::Error) return std::string();
  auto response = m_channel.receive();
  if (!response || response->token != m_token || response->kind != WireKind::Response) {
    if (response) explicit_bzero(response->payload.data(), response->payload.size());
    m_cancelled = true; return std::nullopt;
  }
  return std::move(response->payload);
}
bool WorkerConversation::cancelled() const {
  // During synchronous libpam account/auth calls, cancellation may queue in the
  // socket. Drain it before accepting approval. Late results remain harmless:
  // the native controller has already invalidated its AttemptCoordinator token.
  if (!m_cancelled && m_channel.pending()) {
    auto frame = m_channel.receive();
    if (frame) explicit_bzero(frame->payload.data(), frame->payload.size());
    m_cancelled = true;
  }
  return m_cancelled || m_channel.failed();
}
} // namespace QindaQt::LockAuthentication
