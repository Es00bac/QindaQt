// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <qqmlintegration.h>

namespace QindaQt::Shell::DesktopSurface {

// Presents the user's Desktop-directory contents as desktop-icon rows
// (ADR-0125), replacing the earlier static Places projection: each row is a
// real file or folder read from the Desktop directory instead of the fixed
// XDG-places inventory.
//
// AGENT-CONTRACT: crosses into File Manager exclusively through
// `Apps::FileManager::Desktop::FileBoundary::listLocalFolder` and
// `launchLocalFile` (see module-boundaries.md); it never includes File
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

  [[nodiscard]] QVariantList rows() const { return m_rows; }
  [[nodiscard]] QString feedback() const { return m_feedback; }

  // Re-lists the root directory through the boundary. The desktop context
  // menu's Arrange/Refresh/Clean Up/New Folder entries call this so the icon
  // set reflects the directory's current bounded-local-I/O state.
  Q_INVOKABLE void refresh();
  // Dispatches a bounded local launch for one Desktop entry through
  // FileBoundary::launchLocalFile. The boundary's own typed contract makes
  // this safe for a directory, a since-removed entry, or any other
  // non-regular target: it reports a typed failure instead of throwing,
  // blocking, or opening anything.
  Q_INVOKABLE bool open(const QString &absolutePath);
  Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
  void rowsChanged();
  void feedbackChanged();

private:
  void publishFeedback(const QString &message);

  QString m_root;
  QVariantList m_rows;
  QString m_feedback;
};

} // namespace QindaQt::Shell::DesktopSurface
