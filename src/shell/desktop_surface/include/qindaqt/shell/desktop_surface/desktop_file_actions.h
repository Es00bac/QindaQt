// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <qqmlintegration.h>

namespace QindaQt::Shell::DesktopSurface {

// The File Manager's words for the desktop's menus (ADR-0273). Desktop icons
// are one more File Manager view, so their menus say what a File Manager
// window says: each entry backed by a File Manager action takes its label from
// the File Manager's public menu catalog by action id
// (src/apps/file_manager/public/file_manager_menu_catalog.h), never from QML.
//
// AGENT-CONTRACT: the ids the desktop menus ask for are File Manager action
// ids (ADR-0260). An id the catalog no longer defines answers an empty label,
// which the desktop surface QML rows catch; change both sides together.
// Stateless and GUI-thread only, like every QML singleton.
//
// Not final: QML_ELEMENT instantiates the type through a QQmlElement
// subclass.
class DesktopFileActions : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  explicit DesktopFileActions(QObject *parent = nullptr);

  // The File Manager's label for `actionId` ("file.trash" is "Move to
  // Trash"), or an empty string for an id the catalog does not define.
  Q_INVOKABLE QString label(const QString &actionId) const;
};

} // namespace QindaQt::Shell::DesktopSurface
