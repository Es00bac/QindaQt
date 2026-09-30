// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QUuid>
#include <chrono>
#include <qindaqt/window_management/controller.h>
#include <utility>
namespace QindaQt::WindowManagement {
namespace {
qint64 nowMilliseconds() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
Result rejected(Status status, QString message) {
  return {status, std::move(message), {}, {}, {}, {}};
}
} // namespace
Controller::Controller(Authority &authority, Scene &scene, Executor &executor,
                       Clock clock, Nonce nonce)
    : m_authority(authority), m_scene(scene), m_executor(executor),
      m_clock(clock ? std::move(clock) : Clock(nowMilliseconds)),
      m_nonce(nonce ? std::move(nonce) : Nonce([] {
        return QUuid::createUuid().toString(QUuid::WithoutBraces);
      })) {}
void Controller::expire(qint64 now) {
  for (auto i = m_contexts.begin(); i != m_contexts.end();)
    if (now >= i->expires)
      i = m_contexts.erase(i);
    else
      ++i;
  for (auto i = m_rates.begin(); i != m_rates.end();)
    if (now - i->start >= 1000)
      i = m_rates.erase(i);
    else
      ++i;
}
bool Controller::admit(const QString &owner, qint64 now) {
  if (!m_rates.contains(owner) && m_rates.size() >= 16)
    return false;
  auto &rate = m_rates[owner];
  if (now - rate.start >= 1000)
    rate = {now, 0};
  return ++rate.count <= 8;
}
Result Controller::begin(const QString &owner) {
  if (owner.isEmpty() || !owner.startsWith(u':') ||
      !m_authority.authorized(owner))
    return rejected(
        Status::Denied,
        QStringLiteral("caller is not an enabled command provider"));
  if (m_authority.locked()) {
    invalidate();
    return rejected(Status::Denied, QStringLiteral("session is locked"));
  }
  const auto now = m_clock();
  expire(now);
  if (!admit(owner, now))
    return rejected(Status::ResourceLimit,
                    QStringLiteral("command request rate exceeded"));
  // One fresh hotkey supersedes only this provider's prior context.
  for (auto i = m_contexts.begin(); i != m_contexts.end();)
    if (i->owner == owner)
      i = m_contexts.erase(i);
    else
      ++i;
  if (m_contexts.size() >= 16)
    return rejected(Status::ResourceLimit,
                    QStringLiteral("too many command contexts"));
  const auto snapshot = m_scene.capture();
  if (!snapshot)
    return rejected(Status::Unavailable,
                    QStringLiteral("window-management scene is unavailable"));
  const auto id = m_nonce();
  if (id.isEmpty() || id.size() > 128 || m_contexts.contains(id))
    return rejected(Status::Unavailable,
                    QStringLiteral("command context could not be created"));
  m_contexts.insert(id, {owner, *snapshot, now + 60'000});
  return {Status::Accepted,     {}, {}, id, snapshot->windowId,
          snapshot->containerId};
}
Result Controller::submit(const QString &owner, const QString &id,
                          const QByteArray &wire) {
  if (owner.isEmpty() || !owner.startsWith(u':') ||
      !m_authority.authorized(owner))
    return rejected(
        Status::Denied,
        QStringLiteral("caller is not an enabled command provider"));
  if (m_authority.locked()) {
    invalidate();
    return rejected(Status::Denied, QStringLiteral("session is locked"));
  }
  const auto now = m_clock();
  expire(now);
  if (!admit(owner, now))
    return rejected(Status::ResourceLimit,
                    QStringLiteral("command request rate exceeded"));
  const auto found = m_contexts.constFind(id);
  if (found == m_contexts.cend() || found->owner != owner)
    return rejected(
        Status::Stale,
        QStringLiteral("command context is unavailable or expired"));
  const auto snapshot = found->snapshot;
  // AGENT-GUARD: A submitted context is consumed even when parsing or scene
  // resolution fails. A delayed retry must capture new explicit user intent.
  m_contexts.remove(id);
  if (!m_scene.current(snapshot))
    return rejected(Status::Stale,
                    QStringLiteral("foreground or window ownership changed"));
  QString error;
  const auto command = decodeCommand(wire, &error);
  if (!command)
    return rejected(Status::Invalid, error);
  if (!m_scene.capabilities().contains(operationName(command->operation)))
    return rejected(Status::Unavailable,
                    QStringLiteral("operation is unavailable in this session"));
  const auto target = command->operation == Operation::Launch &&
                              command->target.kind == Target::Kind::Current &&
                              snapshot.windowId.isEmpty()
                          ? Resolution{Status::Accepted, {}, {}, {}}
                          : m_scene.resolve(command->target, snapshot);
  if (target.status != Status::Accepted)
    return {target.status, target.message, target.candidates, {}, {}, {}};
  auto resolved = target.target;
  resolved.contextWindowId = snapshot.windowId;
  resolved.contextContainerId = snapshot.containerId;
  return m_executor.execute(*command, resolved);
}
Result Controller::cancel(const QString &owner, const QString &id) {
  if (!m_authority.authorized(owner))
    return rejected(
        Status::Denied,
        QStringLiteral("caller is not an enabled command provider"));
  const auto found = m_contexts.constFind(id);
  if (found == m_contexts.cend() || found->owner != owner)
    return rejected(Status::Stale,
                    QStringLiteral("command context is unavailable"));
  m_contexts.remove(id);
  return rejected(Status::Cancelled, {});
}
void Controller::invalidate() noexcept { m_contexts.clear(); }
QStringList Controller::capabilities() const { return m_scene.capabilities(); }
} // namespace QindaQt::WindowManagement
