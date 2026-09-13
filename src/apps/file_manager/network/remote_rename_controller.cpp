// SPDX-License-Identifier: GPL-3.0-or-later
#include "remote_rename_controller.h"
#include "network_location.h"
#include "remote_renamer.h"

#include <QFile>

#include <algorithm>

namespace QindaQt::Apps::FileManager {

namespace {

// Mirrors mutation_controller.cpp's validName(): remote entries validate
// through the same sibling-name rules so a local and remote rename accept
// exactly the same names (separators and dot names rejected).
[[nodiscard]] bool validRemoteName(const QString &name) {
  return !name.isEmpty() && QFile::encodeName(name).size() <= 255 &&
         name != QLatin1String(".") &&
         name != QLatin1String("..") && !name.contains(QLatin1Char('/')) &&
         !name.contains(QLatin1Char('\\')) && !name.contains(QChar::Null);
}

} // namespace

RemoteRenameController::RemoteRenameController(RemoteRenamer &renamer, QObject *parent)
    : QObject(parent), m_renamer(&renamer) {
  connect(&renamer, &RemoteRenamer::renameFinished, this,
          [this](quint64 generation, const QString &diagnostic) {
            onRenameFinished(generation, diagnostic);
          });
}

bool RemoteRenameController::requestRename(const QVector<DirectoryEntry> &listedEntries,
                                           const QUrl &remoteUrl, quint64 listingGeneration,
                                           const QString &sourcePath, const QString &newName) {
  const auto refuse = [this](const QString &message) {
    Q_EMIT failure(message);
    return false;
  };
  if (m_busy) {
    return refuse(QStringLiteral("A remote rename is already in progress"));
  }
  if (!validRemoteName(newName)) {
    return refuse(QStringLiteral("Choose a valid item name"));
  }
  const QUrl source(sourcePath);
  // The source must be a currently listed child of the active remote
  // directory: this rejects unlisted/stale identities, cross-folder URLs,
  // userinfo, and every non-canonical spelling before any dispatch.
  const auto listed =
      std::find_if(listedEntries.begin(), listedEntries.end(),
                   [&sourcePath](const DirectoryEntry &entry) {
                     return entry.absolutePath == sourcePath;
                   });
  if (listed == listedEntries.end()) {
    return refuse(QStringLiteral("The item is no longer listed in this folder"));
  }
  if (!NetworkLocation::isSupportedScheme(source) || !source.userName().isEmpty() ||
      !source.password().isEmpty() || source.scheme() != remoteUrl.scheme() ||
      source.host() != remoteUrl.host() || source.port() != remoteUrl.port() ||
      NetworkLocation::parentOf(source) != remoteUrl) {
    return refuse(QStringLiteral("Rename must stay in the current folder"));
  }
  // AGENT-NOTE: mirrors MutationController::renameItem's unchanged-default
  // no-op so accepting the dialog's prefilled name is a true no-op.
  if (newName == listed->name) {
    return true;
  }
  m_busy = true;
  m_generation = listingGeneration;
  Q_EMIT busyChanged();
  m_renamer->rename(m_generation, source, NetworkLocation::childUrl(remoteUrl, newName));
  return true;
}

void RemoteRenameController::onRenameFinished(quint64 generation,
                                              const QString &diagnostic) {
  // Fenced like the listing handler: a result belonging to a cancelled or
  // superseded rename (including the quiet kill that cancellation triggers)
  // is discarded without touching visible state.
  if (!m_busy || generation != m_generation) {
    return;
  }
  m_busy = false;
  m_generation = 0;
  Q_EMIT busyChanged();
  if (diagnostic.isEmpty()) {
    // Success: the caller refreshes the authoritative listing from the
    // server -- the displayed name never changes optimistically here.
    Q_EMIT refreshRequested();
    return;
  }
  Q_EMIT failure(diagnostic);
}

void RemoteRenameController::cancelPending() {
  if (!m_busy) {
    return;
  }
  m_renamer->cancel(m_generation);
  m_busy = false;
  m_generation = 0;
  Q_EMIT busyChanged();
}

} // namespace QindaQt::Apps::FileManager
