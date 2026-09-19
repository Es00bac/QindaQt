// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewer_actions.h"
#include <QCoreApplication>

namespace QindaQt::Viewer {
namespace {
AppShell::ActionSpec action(const char *id, const char *menu, const char *menuLabel,
                           const char *label, const char *shortcut, int order)
{
    AppShell::ActionSpec value;
    value.id = QString::fromLatin1(id);
    value.menuId = QString::fromLatin1(menu);
    value.menuLabel = QCoreApplication::translate("ViewerActions", menuLabel);
    value.label = QCoreApplication::translate("ViewerActions", label);
    value.shortcut = QKeySequence(QString::fromLatin1(shortcut));
    value.order = order;
    value.menuOrder = value.menuId == QStringLiteral("file") ? 0 : 1;
    return value;
}
} // namespace
QList<AppShell::ActionSpec> viewerActions()
{
    return {
        action("file.open", "file", "File", "Open…", "Ctrl+O", 0),
        action("file.close", "file", "File", "Close document", "Ctrl+W", 1),
        action("file.quit", "file", "File", "Quit", "Ctrl+Q", 2),
        action("view.previous", "view", "View", "Previous page", "PgUp", 0),
        action("view.next", "view", "View", "Next page", "PgDown", 1),
        action("view.first", "view", "View", "First page", "Ctrl+Home", 2),
        action("view.last", "view", "View", "Last page", "Ctrl+End", 3),
        action("view.zoom-in", "view", "View", "Zoom in", "Ctrl++", 4),
        action("view.zoom-out", "view", "View", "Zoom out", "Ctrl+-", 5),
        action("view.actual", "view", "View", "Actual size", "Ctrl+0", 6),
        action("view.fit", "view", "View", "Fit page", "Ctrl+1", 7),
        action("view.width", "view", "View", "Fit width", "Ctrl+2", 8),
        action("view.rotate", "view", "View", "Rotate clockwise", "Ctrl+R", 9),
    };
}
} // namespace QindaQt::Viewer
