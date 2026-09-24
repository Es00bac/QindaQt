// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screensaver_idle_lock_inhibitor_p.h"

#include <QCoreApplication>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QEventLoop>
#include <QTimer>
#include <QUuid>

namespace QindaQt::Apps::SettingsScreensaver {
namespace {

constexpr auto kScreenSaverService = "org.freedesktop.ScreenSaver";
constexpr auto kScreenSaverPath = "/ScreenSaver";
constexpr auto kScreenSaverInterface = "org.freedesktop.ScreenSaver";
constexpr int kDbusCallTimeoutMilliseconds = 2'000;

QString translated(const char *sourceText) {
  return QCoreApplication::translate("ProcessScreensaverPreview", sourceText);
}

std::optional<QDBusMessage> callWithTimeout(QDBusConnection &bus,
                                            const QDBusMessage &request) {
  QDBusPendingCallWatcher watcher(
      bus.asyncCall(request, kDbusCallTimeoutMilliseconds));
  QEventLoop loop;
  QTimer deadline;
  deadline.setSingleShot(true);
  QObject::connect(&watcher, &QDBusPendingCallWatcher::finished, &loop,
                   &QEventLoop::quit);
  QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
  deadline.start(kDbusCallTimeoutMilliseconds);
  if (!watcher.isFinished()) {
    // AGENT-GUARD: pump the bus so same-process fake services and real session
    // authorities can answer. Excluding user input prevents a nested preview
    // start or dismissal while the lock guarantee is being established.
    loop.exec(QEventLoop::ExcludeUserInputEvents);
  }
  if (!watcher.isFinished()) {
    return std::nullopt;
  }
  return watcher.reply();
}

} // namespace

ProcessScreensaverPreview::ScreensaverPreviewIdleLockInhibitor::
    ~ScreensaverPreviewIdleLockInhibitor() {
  release();
}

bool ProcessScreensaverPreview::ScreensaverPreviewIdleLockInhibitor::acquire(
    QString *error) {
  if (m_cookie.has_value()) {
    if (error != nullptr) {
      *error = translated("Automatic screen locking is already inhibited.");
    }
    return false;
  }

  // AGENT-CONTRACT: KScreenLocker owns the automatic idle lock and honors
  // this cookie before its idle timeout. Preview must fail closed when that
  // authority cannot confirm the request (ADR-0259).
  m_connectionName =
      QStringLiteral("qindaqt-preview-%1")
          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
  m_bus.emplace(QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                              m_connectionName));
  if (!m_bus->isConnected()) {
    if (error != nullptr) {
      *error = translated("Automatic screen locking could not be inhibited; "
                          "the preview was not started.");
    }
    disconnectBus();
    return false;
  }

  QDBusMessage request = QDBusMessage::createMethodCall(
      QString::fromLatin1(kScreenSaverService),
      QString::fromLatin1(kScreenSaverPath),
      QString::fromLatin1(kScreenSaverInterface), QStringLiteral("Inhibit"));
  request << QStringLiteral("org.qindaqt.Settings")
          << translated("Previewing a screen saver");
  const std::optional<QDBusMessage> reply = callWithTimeout(*m_bus, request);
  if (!reply.has_value() || reply->type() == QDBusMessage::ErrorMessage ||
      reply->arguments().size() != 1) {
    if (error != nullptr) {
      *error =
          translated("Automatic screen locking could not be inhibited; the "
                     "preview was not started: %1")
              .arg(
                  !reply.has_value()
                      ? translated("the lock service did not reply in time")
                      : (reply->errorMessage().isEmpty()
                             ? translated(
                                   "the lock service returned an invalid reply")
                             : reply->errorMessage()));
    }
    disconnectBus();
    return false;
  }

  bool converted = false;
  const uint cookie = reply->arguments().constFirst().toUInt(&converted);
  if (!converted) {
    if (error != nullptr) {
      *error = translated("Automatic screen locking could not be inhibited; "
                          "the lock service returned no cookie.");
    }
    disconnectBus();
    return false;
  }
  m_cookie = cookie;
  return true;
}

void ProcessScreensaverPreview::ScreensaverPreviewIdleLockInhibitor::release() {
  if (m_cookie.has_value() && m_bus.has_value() && m_bus->isConnected()) {
    QDBusMessage request = QDBusMessage::createMethodCall(
        QString::fromLatin1(kScreenSaverService),
        QString::fromLatin1(kScreenSaverPath),
        QString::fromLatin1(kScreenSaverInterface),
        QStringLiteral("UnInhibit"));
    request << *m_cookie;
    // AGENT-GUARD: dropping this dedicated connection also releases the
    // KScreenLocker request if UnInhibit cannot complete. Its service watcher
    // removes requests when the caller's unique bus name vanishes.
    callWithTimeout(*m_bus, request);
  }
  m_cookie.reset();
  disconnectBus();
}

void ProcessScreensaverPreview::ScreensaverPreviewIdleLockInhibitor::
    disconnectBus() {
  m_bus.reset();
  if (!m_connectionName.isEmpty()) {
    QDBusConnection::disconnectFromBus(m_connectionName);
    m_connectionName.clear();
  }
}

} // namespace QindaQt::Apps::SettingsScreensaver
