// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/exporter/menu_source.h>

#include <QtCore/QPointer>
#include <QtCore/QUuid>

class QMenuBar;

namespace QindaQt::Shell::GlobalMenu::QtWidgetsAdapter
{

// Native Qt export: walks a real `QMenuBar`/`QMenu`/`QAction` tree — the
// exact shape the integrated Text Editor exposes per ADR-0022 — into the
// canonical model. This is the only target in `global_menu` that links
// `Qt6::Widgets`; the shell itself stays Widgets-free.
//
// AGENT-CONTRACT: an item's id is `QAction::objectName()` when the app sets
// one (ADR-0022 requires this for Text Editor's commands). Without an
// object name, id falls back to a pre-order position token that is only
// stable while sibling structure does not change; apps that want delta
// stability across menu edits must set persistent object names.
class QMenuBarMenuSource final : public Exporter::MenuSource
{
public:
    QMenuBarMenuSource(QMenuBar *menuBar, QUuid ownerWindowId);

    [[nodiscard]] Protocol::MenuTree snapshot() const override;

private:
    QPointer<QMenuBar> m_menuBar;
    QUuid m_ownerWindowId;
};

} // namespace QindaQt::Shell::GlobalMenu::QtWidgetsAdapter
