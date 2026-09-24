// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_manager1_service.h"

#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusReply>

namespace QindaQt::Apps::FileManager {

QString FileManager1Service::serviceName() {
  return QStringLiteral("org.freedesktop.FileManager1");
}

QString FileManager1Service::objectPath() {
  return QStringLiteral("/org/freedesktop/FileManager1");
}

FileManager1Service::FileManager1Service(RevealWindows &windows, QObject *parent)
    : QObject(parent), m_windows(windows) {}

bool FileManager1Service::publish(QDBusConnection connection) {
  if (!connection.isConnected() || connection.interface() == nullptr) {
    return false;
  }
  if (!connection.registerObject(objectPath(), this, QDBusConnection::ExportAllSlots)) {
    return false;
  }
  // AGENT-NOTE: queue, never replace. Another file manager that already
  // serves the name keeps it; each further File Manager process waits in line
  // so the service survives the first window closing.
  const QDBusReply<QDBusConnectionInterface::RegisterServiceReply> reply =
      connection.interface()->registerService(serviceName(),
                                              QDBusConnectionInterface::QueueService,
                                              QDBusConnectionInterface::DontAllowReplacement);
  if (!reply.isValid() || reply.value() == QDBusConnectionInterface::ServiceNotRegistered) {
    connection.unregisterObject(objectPath());
    return false;
  }
  return true;
}

void FileManager1Service::ShowFolders(const QStringList &uris, const QString &startupId) {
  reveal(RevealKind::Folders, uris, startupId);
}

void FileManager1Service::ShowItems(const QStringList &uris, const QString &startupId) {
  reveal(RevealKind::Items, uris, startupId);
}

void FileManager1Service::ShowItemProperties(const QStringList &uris,
                                             const QString &startupId) {
  reveal(RevealKind::ItemProperties, uris, startupId);
}

void FileManager1Service::reveal(RevealKind kind, const QStringList &uris,
                                 const QString &startupId) {
  const RevealPlan plan = planReveal(kind, uris);
  if (!plan.ok()) {
    if (calledFromDBus()) {
      sendErrorReply(QDBusError::InvalidArgs, plan.diagnostic);
    }
    return;
  }
  bool shownAll = true;
  bool first = true;
  for (const RevealRequest &request : plan.requests) {
    // An activation token is single-use: only the first window may raise
    // itself with it; later windows open without stealing focus.
    shownAll = m_windows.show(request, first ? startupId : QString()) && shownAll;
    first = false;
  }
  if (!shownAll && calledFromDBus()) {
    sendErrorReply(QDBusError::Failed,
                   QStringLiteral("QindaQt File Manager could not show every folder"));
  }
}

} // namespace QindaQt::Apps::FileManager
