// SPDX-License-Identifier: GPL-3.0-or-later
#include "attachment_proof_p.h"
#include <QDBusPendingCallWatcher>
#include <QRegularExpression>
#include <optional>
#include <qindaqt/compositor_names/compositor_names.h>
#include <unistd.h>
namespace QindaQt::Platform::Compositor::Private {
namespace {
enum Step { Owner, Pid, Uid, SessionRegistered, SessionUid, StepCount };
bool uniqueName(const QString &name) {
  static const QRegularExpression pattern(QStringLiteral("^:[0-9]+\\.[0-9]+$"));
  return name.size() <= 255 && pattern.match(name).hasMatch();
}
QDBusMessage daemonCall(const QString &method, const QString &argument) {
  auto message = QDBusMessage::createMethodCall(
      QStringLiteral("org.freedesktop.DBus"),
      QStringLiteral("/org/freedesktop/DBus"),
      QStringLiteral("org.freedesktop.DBus"), method);
  message << argument;
  return message;
}
QDBusMessage request(int step, const ProofFacts &facts,
                     const QString &session) {
  switch (step) {
  case Owner:
    return daemonCall(QStringLiteral("GetNameOwner"),
                      QString(CompositorNames::service));
  case Pid:
    return daemonCall(QStringLiteral("GetConnectionUnixProcessID"),
                      facts.compositorOwner);
  case Uid:
    return daemonCall(QStringLiteral("GetConnectionUnixUser"),
                      facts.compositorOwner);
  case SessionRegistered:
    return daemonCall(QStringLiteral("NameHasOwner"), session);
  default:
    return daemonCall(QStringLiteral("GetConnectionUnixUser"), session);
  }
}
bool shaped(const QDBusMessage &reply, const char *signature) {
  return reply.type() == QDBusMessage::ReplyMessage &&
         reply.signature() == QLatin1String(signature) &&
         reply.arguments().size() == 1;
}
ProofResult retry(const QString &reason) {
  return {ProofOutcome::Retry, {}, reason};
}
ProofResult retire(const QString &reason) {
  return {ProofOutcome::Retire, {}, reason};
}
QString failure(const QDBusMessage &reply, const QString &what) {
  return reply.type() == QDBusMessage::ErrorMessage
             ? QStringLiteral("%1 failed: %2").arg(what, reply.errorName())
             : QStringLiteral("%1 returned a malformed reply").arg(what);
}
// nullopt: continue with the next step.
std::optional<ProofResult> apply(int step, const QDBusMessage &reply,
                                 ProofFacts &facts) {
  const bool vanished =
      reply.type() == QDBusMessage::ErrorMessage &&
      reply.errorName() ==
          QStringLiteral("org.freedesktop.DBus.Error.NameHasNoOwner");
  switch (step) {
  case Owner: {
    if (vanished)
      return retry(QStringLiteral("no process owns the compositor bus name"));
    if (!shaped(reply, "s"))
      return retry(failure(reply, QStringLiteral("compositor owner query")));
    facts.compositorOwner = reply.arguments().first().toString();
    if (!uniqueName(facts.compositorOwner))
      return retry(QStringLiteral("compositor owner is not a unique name"));
    return std::nullopt;
  }
  case Pid:
    if (!shaped(reply, "u") || reply.arguments().first().toUInt() <= 1)
      return retry(failure(reply, QStringLiteral("compositor PID query")));
    facts.compositorPid = reply.arguments().first().toUInt();
    return std::nullopt;
  case Uid:
    if (!shaped(reply, "u"))
      return retry(failure(reply, QStringLiteral("compositor UID query")));
    if (reply.arguments().first().toUInt() != geteuid())
      return retry(QStringLiteral("compositor owner belongs to another user"));
    return std::nullopt;
  case SessionRegistered:
    if (!shaped(reply, "b"))
      return retry(failure(reply, QStringLiteral("session owner query")));
    if (!reply.arguments().first().toBool())
      return retire(QStringLiteral("selected session owner disconnected"));
    return std::nullopt;
  default:
    if (vanished)
      return retire(QStringLiteral("selected session owner disconnected"));
    if (!shaped(reply, "u"))
      return retry(failure(reply, QStringLiteral("session UID query")));
    if (reply.arguments().first().toUInt() != geteuid())
      return retire(QStringLiteral("selected session owner belongs to another user"));
    return ProofResult{ProofOutcome::Proven, facts, {}};
  }
}
} // namespace
ProofResult proveSynchronously(const QDBusConnection &bus,
                               const QString &sessionOwner, int timeoutMs) {
  ProofFacts facts;
  for (int step = Owner; step < StepCount; ++step) {
    const auto reply =
        bus.call(request(step, facts, sessionOwner), QDBus::Block, timeoutMs);
    if (auto result = apply(step, reply, facts))
      return *result;
  }
  return retry(QStringLiteral("incomplete proof"));
}
AsyncProof::AsyncProof(QDBusConnection bus, QString sessionOwner,
                       int timeoutMs, Completion completion, QObject *parent)
    : QObject(parent), m_bus(std::move(bus)),
      m_sessionOwner(std::move(sessionOwner)), m_timeoutMs(timeoutMs),
      m_completion(std::move(completion)) {}
AsyncProof::~AsyncProof() { cancel(); }
void AsyncProof::start() {
  if (m_done || m_pending)
    return;
  m_step = Owner;
  m_facts = {};
  issue();
}
void AsyncProof::cancel() {
  m_done = true;
  if (m_pending) {
    m_pending->disconnect(this);
    m_pending->deleteLater();
    m_pending.clear();
  }
}
void AsyncProof::issue() {
  if (!m_bus.isConnected()) {
    finish(retry(QStringLiteral("bus connection is closed")));
    return;
  }
  auto *watcher = new QDBusPendingCallWatcher(
      m_bus.asyncCall(request(m_step, m_facts, m_sessionOwner), m_timeoutMs),
      this);
  m_pending = watcher;
  connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
    const auto reply = watcher->reply();
    watcher->deleteLater();
    if (m_done || m_pending != watcher)
      return;
    m_pending.clear();
    if (auto result = apply(m_step, reply, m_facts)) {
      finish(*result);
      return;
    }
    ++m_step;
    issue();
  });
}
void AsyncProof::finish(const ProofResult &result) {
  if (m_done)
    return;
  m_done = true;
  // Completion may cancel or schedule deletion of this object; it runs last.
  if (m_completion)
    m_completion(result);
}
} // namespace QindaQt::Platform::Compositor::Private
