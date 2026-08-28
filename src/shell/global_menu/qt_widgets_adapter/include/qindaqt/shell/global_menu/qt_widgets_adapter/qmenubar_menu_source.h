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
//
// AGENT-GUARD: this adapter never truncates. A tree that would exceed any
// canonical bound (depth, siblings, total items), revisits a submenu
// (cycle), or loses its menu bar mid-walk yields an INCOMPLETE
// MenuSnapshot; the exporter discards incomplete snapshots whole, so a
// hostile or degenerate application menu can never surface as a bounded
// prefix of the truth.
//
// Lifetime/thread/mutation contract: the observed QMenuBar (and every menu
// reachable from it) must outlive the QMenuBarMenuSource, snapshot() must
// be called on the Qt GUI thread that owns the widget tree, and the menu
// structure must not be mutated while snapshot() is running. Mutating the
// structure concurrently with a snapshot is unsupported; the adapter's
// cycle/duplicate detection bounds the damage to an incomplete snapshot,
// never a wrong "complete" one.
class QMenuBarMenuSource final : public Exporter::MenuSource
{
public:
    QMenuBarMenuSource(QMenuBar *menuBar, QUuid ownerWindowId);

    [[nodiscard]] Exporter::MenuSnapshot snapshot() const override;

private:
    QPointer<QMenuBar> m_menuBar;
    QUuid m_ownerWindowId;
};

} // namespace QindaQt::Shell::GlobalMenu::QtWidgetsAdapter
