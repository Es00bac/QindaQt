// SPDX-License-Identifier: GPL-3.0-or-later
#include "remote_create_folder_controller.h"
#include "network_location.h"
#include "remote_folder_creator.h"

#include <QFile>

namespace QindaQt::Apps::FileManager {

namespace {

// Same sibling-name rules as local folder creation and remote rename
// (mutation_controller.cpp's validName / RemoteRenameController): remote
// folders accept exactly the same names -- separators, dot names, and NUL
// rejected, one-component names only.
[[nodiscard]] bool validRemoteFolderName(const QString &name) {
  return !name.isEmpty() && QFile::encodeName(name).size() <= 255 &&
         name != QLatin1String(".") &&
         name != QLatin1String("..") && !name.contains(QLatin1Char('/')) &&
         !name.contains(QLatin1Char('\\')) && !name.contains(QChar::Null);
}

} // namespace

RemoteCreateFolderController::RemoteCreateFolderController(RemoteFolderCreator &creator,
                                                           QObject *parent)
    : QObject(parent), m_creator(&creator) {
  connect(&creator, &RemoteFolderCreator::createFinished, this,
          [this](quint64 generation, const QString &diagnostic) {
            onCreateFinished(generation, diagnostic);
          });
}

bool RemoteCreateFolderController::requestCreate(const QUrl &remoteUrl,
                                                 quint64 listingGeneration,
                                                 const QString &name) {
  const auto refuse = [this](const QString &message) {
    Q_EMIT failure(message);
    return false;
  };
  if (m_busy) {
    return refuse(QStringLiteral("A remote folder creation is already in progress"));
  }
  if (!validRemoteFolderName(name)) {
    return refuse(QStringLiteral("Choose a valid folder name"));
  }
  // AGENT-GUARD: the target must be a one-level child of the active remote
  // directory -- parentOf() refuses anything already at or above the
  // authority root, so no traversal can leave the current folder.
  const QUrl target = NetworkLocation::childUrl(remoteUrl, name);
  if (!NetworkLocation::isSupportedScheme(target) ||
      NetworkLocation::parentOf(target) != remoteUrl) {
    return refuse(QStringLiteral("Create the folder in the current directory"));
  }
  m_busy = true;
  m_generation = listingGeneration;
  Q_EMIT busyChanged();
  m_creator->createFolder(m_generation, target);
  return true;
}

void RemoteCreateFolderController::onCreateFinished(quint64 generation,
                                                    const QString &diagnostic) {
  // Fenced like the rename handler: a result belonging to a cancelled or
  // superseded create (including the quiet kill that cancellation
  // triggers) is discarded without touching visible state.
  if (!m_busy || generation != m_generation) {
    return;
  }
  m_busy = false;
  m_generation = 0;
  Q_EMIT busyChanged();
  if (diagnostic.isEmpty()) {
    // Success: the caller refreshes the authoritative listing from the
    // server -- no optimistic entry is displayed before this point.
    Q_EMIT refreshRequested();
    return;
  }
  Q_EMIT failure(diagnostic);
}

void RemoteCreateFolderController::cancelPending() {
  if (!m_busy) {
    return;
  }
  m_creator->cancel(m_generation);
  m_busy = false;
  m_generation = 0;
  Q_EMIT busyChanged();
}

} // namespace QindaQt::Apps::FileManager
