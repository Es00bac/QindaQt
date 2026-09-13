// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <qqmlintegration.h>

#include <optional>

namespace QindaQt::Shell::DesktopSurface {

// Presents the user's Desktop-directory contents as desktop-icon rows
// (ADR-0125), replacing the earlier static Places projection: each row is a
// real file or folder read from the Desktop directory instead of the fixed
// XDG-places inventory.
//
// AGENT-CONTRACT: crosses into File Manager exclusively through
// `Apps::FileManager::Desktop::FileBoundary::listLocalFolder`,
// `launchLocalFile`, and `openLocalFolder` (see module-boundaries.md); it
// never includes File
// Manager's model/**, mutation/**, or app_shell/** headers directly. A
// missing/unreadable root publishes zero rows plus `feedback` instead of
// leaving the surface input-blocked; hidden (dot) entries are omitted from
// the desktop presentation the same way every other stock desktop hides
// them.
//
// Not final: QML_ELEMENT instantiates the type through a QQmlElement
// subclass.
class DesktopContentsController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  // Rows: {id, label, path, iconName, accessibleName, isDirectory}, one per
  // visible (non-hidden) Desktop-directory child, directories first.
  Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
  // Last listing/launch diagnostic; empty when the last operation succeeded.
  Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)

public:
  explicit DesktopContentsController(QObject *parent = nullptr);
  // Test seam: lists `root` instead of QStandardPaths::DesktopLocation, so
  // tests exercise a real temporary directory without touching the user's
  // home.
  explicit DesktopContentsController(QString root, QObject *parent = nullptr);
  // Test seam: also replaces FileBoundary::fileManagerProgramCandidates() so a
  // folder activation starts a recording stand-in instead of File Manager.
  DesktopContentsController(QString root, QStringList fileManagerPrograms,
                            QObject *parent = nullptr);

  [[nodiscard]] QVariantList rows() const { return m_rows; }
  [[nodiscard]] QString feedback() const { return m_feedback; }

  // Re-lists the root directory through the boundary. The desktop context
  // menu's Arrange/Refresh/Clean Up/New Folder entries call this so the icon
  // set reflects the directory's current bounded-local-I/O state.
  Q_INVOKABLE void refresh();
  // Activates one entry from the last listing. A listed directory opens in
  // QindaQt File Manager through FileBoundary::openLocalFolder, fenced by the
  // identity the listing reported; a listed regular file keeps the bounded
  // default-handler launch through FileBoundary::launchLocalFile. A path the
  // listing never reported, or any typed boundary refusal, publishes
  // `feedback`, returns false, and launches nothing.
  Q_INVOKABLE bool open(const QString &absolutePath);
  Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
  void rowsChanged();
  void feedbackChanged();

private:
  struct ListedEntry {
    bool isDirectory = false;
    quint64 device = 0;
    quint64 inode = 0;
  };

  void publishFeedback(const QString &message);

  QString m_root;
  // Unset means FileBoundary's production program candidates.
  std::optional<QStringList> m_fileManagerPrograms;
  QHash<QString, ListedEntry> m_listed;
  QVariantList m_rows;
  QString m_feedback;
};

} // namespace QindaQt::Shell::DesktopSurface
