// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../model/reveal_request.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QObject>
#include <QString>
#include <QStringList>

namespace QindaQt::Apps::FileManager {

// Where FileManager1Service shows a planned request (ADR-0273): the window
// factory. Production is ProcessRevealWindows; tests pass a recording fake.
class RevealWindows {
public:
  virtual ~RevealWindows() = default;

  // Shows request.folder with request.names selected, and their properties
  // when request.showProperties. `activationToken` is the caller's StartupId
  // (an xdg-activation token, or an X11 startup id), possibly empty. Returns
  // false when no window could be shown.
  [[nodiscard]] virtual bool show(const RevealRequest &request,
                                  const QString &activationToken) = 0;
};

// AGENT-CONTRACT (ADR-0273): File Manager's side of the freedesktop.org
// org.freedesktop.FileManager1 interface at /org/freedesktop/FileManager1,
// which browsers, editors and download managers call for "Show in folder".
// Every URI of a call passes planReveal() before anything is shown. A refused
// call answers org.freedesktop.DBus.Error.InvalidArgs so the caller falls back
// to its own behaviour; a request no window could show answers
// org.freedesktop.DBus.Error.Failed. The bus name is queued, never taken: the
// first File Manager process serves and the next takes over when it exits.
// GUI-thread only; `windows` is borrowed and must outlive this object.
class FileManager1Service final : public QObject, protected QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.FileManager1")

public:
  [[nodiscard]] static QString serviceName();
  [[nodiscard]] static QString objectPath();

  explicit FileManager1Service(RevealWindows &windows, QObject *parent = nullptr);

  // Exports this object on `connection` and queues for serviceName() without
  // replacing a current owner. False when the bus is unusable; the window
  // then simply works without the service.
  bool publish(QDBusConnection connection);

public slots:
  void ShowFolders(const QStringList &uris, const QString &startupId);
  void ShowItems(const QStringList &uris, const QString &startupId);
  void ShowItemProperties(const QStringList &uris, const QString &startupId);

private:
  void reveal(RevealKind kind, const QStringList &uris, const QString &startupId);

  RevealWindows &m_windows;
};

} // namespace QindaQt::Apps::FileManager
