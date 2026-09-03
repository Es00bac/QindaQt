// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>

namespace QindaQt::Shell::Launcher {

// Result of requesting D-Bus activation. accepted means the request was
// well-formed and handed to the bus; completion truth arrives through the
// activator's activationFinished signal. accepted=false is a local,
// synchronous refusal (invalid identity, no bus).
struct ActivationDispatch {
  bool accepted = false;
  QString diagnostic;

  friend bool operator==(const ActivationDispatch &,
                         const ActivationDispatch &) = default;
};

// D-Bus activation seam for DBusActivatable=true desktop entries
// (org.freedesktop.Application). Tests inject a fake; production uses
// SessionBusActivator. No activation tokens are sent yet — startup
// notification integration is a later slice (see ADR-0062).
class LaunchActivator : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  ~LaunchActivator() override = default;

  virtual ActivationDispatch activate(const QString &desktopId,
                                      const QString &actionId) = 0;

Q_SIGNALS:
  void activationFinished(const QString &desktopId, bool ok,
                          const QString &diagnostic);
};

// Production activator on an injected bus connection (the composition root
// passes the session bus; tests pass a fake activator and never touch one).
// The service name is the desktop-entry id; the object path is the id with
// dots replaced by slashes, per the D-Bus activation specification.
class SessionBusActivator final : public LaunchActivator {
  Q_OBJECT
public:
  explicit SessionBusActivator(const QDBusConnection &connection,
                               QObject *parent = nullptr);

  ActivationDispatch activate(const QString &desktopId,
                              const QString &actionId) override;

private:
  QDBusConnection m_connection;
};

} // namespace QindaQt::Shell::Launcher
