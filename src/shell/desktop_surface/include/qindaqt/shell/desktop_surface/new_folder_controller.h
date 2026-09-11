// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <qqmlintegration.h>

namespace QindaQt::Shell::DesktopSurface {

// Creates uniquely named folders under the user's Desktop directory for the
// desktop context menu's "New Folder" action (ADR-0125). Registered into the
// QindaQt.Shell.DesktopSurface module so the QML owns the instance.
//
// AGENT-GUARD: writes ONLY under QStandardPaths::DesktopLocation, and every
// name passes the plain-file-name guard (no separators, no "..", no absolute
// or dot names) before any filesystem call. Generated names pass by
// construction; the guard exists so the contract holds even if a future
// caller forwards user text into create(). Violating it puts shell-writable
// directories outside the Desktop and breaks the desktop surface's
// least-privilege story.
//
// ADR-0125 note: this audited-builtin presentation behavior performs a
// filesystem write without a capability enum — the desktop-icons manifest
// grants no filesystem capability because the write belongs to the audited
// shell package itself, not to an applet grant. Recorded here for the
// ADR-0125 desktop-zone text.
//
// Not final: QML_ELEMENT instantiates the type through a QQmlElement
// subclass.
class NewFolderController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  // Last failure diagnostic; empty after a successful create().
  Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)

public:
  explicit NewFolderController(QObject *parent = nullptr);

  // Creates "New Folder", "New Folder 2", ... under the Desktop directory.
  // With an argument, creates that name instead (same guard; used by tests
  // to pin the traversal-refusal contract). Returns the created absolute
  // path, or an empty string after publishing `feedback`.
  Q_INVOKABLE QString create(const QString &requestedName = {});
  Q_INVOKABLE void clearFeedback();
  [[nodiscard]] QString feedback() const { return m_feedback; }

Q_SIGNALS:
  void feedbackChanged();

private:
  void publishFeedback(const QString &message);

  QString m_feedback;
};

} // namespace QindaQt::Shell::DesktopSurface
