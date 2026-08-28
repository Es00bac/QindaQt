// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/qt_widgets_adapter/qmenubar_menu_source.h>

#include <qindaqt/shell/global_menu/exporter/menu_source.h>
#include <qindaqt/shell/global_menu/protocol/menu_limits.h>

#include <QtCore/QSet>

#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <QtGui/QKeySequence>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>

namespace QindaQt::Shell::GlobalMenu::QtWidgetsAdapter
{

namespace
{

using Protocol::MenuItem;
using Protocol::MenuItemKind;

// Stable defect diagnostics published through MenuSnapshot::defectCode.
// AGENT-NOTE: the distinct codes let tests and support tell hostile menu
// sizes apart from object-lifetime losses; keep them stable.
inline constexpr auto kDefectTooDeep = "too-deep";
inline constexpr auto kDefectTooManyChildren = "too-many-children";
inline constexpr auto kDefectTooManyItems = "too-many-items";
inline constexpr auto kDefectSubmenuCycle = "submenu-cycle";
inline constexpr auto kDefectSourceDestroyed = "source-destroyed";

// Splits Qt's mnemonic convention ('&x' marks 'x' as the mnemonic, '&&' is a
// literal ampersand) into plain display text plus a UTF-16 offset, matching
// the canonical model's toolkit-neutral shape.
void extractMnemonic(const QString &raw, QString &plainOut, int &mnemonicIndexOut)
{
    plainOut.clear();
    plainOut.reserve(raw.size());
    mnemonicIndexOut = -1;

    const qsizetype length = raw.size();
    for (qsizetype i = 0; i < length; ++i) {
        const QChar c = raw.at(i);
        if (c == QLatin1Char('&') && i + 1 < length) {
            const QChar next = raw.at(i + 1);
            if (next == QLatin1Char('&')) {
                plainOut.append(QLatin1Char('&'));
                ++i;
                continue;
            }
            if (mnemonicIndexOut == -1) {
                mnemonicIndexOut = static_cast<int>(plainOut.size());
            }
            continue;
        }
        plainOut.append(c);
    }
}

QString radioGroupIdFor(const QAction *action)
{
    QActionGroup *group = action->actionGroup();
    if (!group || !group->isExclusive()) {
        return {};
    }
    if (!group->objectName().isEmpty()) {
        return group->objectName();
    }
    // AGENT-NOTE: apps that want portable, delta-stable radio-group identity
    // should call QActionGroup::setObjectName. This pointer-derived fallback
    // is only stable within one process lifetime.
    return QStringLiteral("qam-group:%1").arg(reinterpret_cast<quintptr>(group));
}

// Shared mutable traversal budget/state. AGENT-GUARD: every budget breach
// records its distinct defect code and abandons the snapshot as incomplete
// instead of trimming; the exporter treats an incomplete snapshot as a
// whole-tree rejection.
struct WalkState {
    int totalItems = 0;
    QSet<const QMenu *> visitedMenus;
    QString defectCode;
};

bool buildItem(const QAction *action, int depth, int &fallbackIdCounter, WalkState &state,
               MenuItem &out)
{
    if (++state.totalItems > Protocol::kMaxTotalItems) {
        state.defectCode = QLatin1String(kDefectTooManyItems);
        return false;
    }
    if (depth > Protocol::kMaxDepth) {
        state.defectCode = QLatin1String(kDefectTooDeep);
        return false;
    }

    QMenu *submenu = action->menu();

    // AGENT-NOTE: a top-level menu title's QAction is created internally by
    // QMenuBar::addMenu()/QMenu::addMenu() and is rarely named directly; the
    // integrated Text Editor (ADR-0022) names the QMenu instead
    // (`fileMenu->setObjectName(...)`). Prefer the action's own name, then
    // fall back to the submenu's, before the positional fallback below.
    QString id = action->objectName();
    if (id.isEmpty() && submenu && !submenu->objectName().isEmpty()) {
        id = submenu->objectName();
    }
    if (id.isEmpty()) {
        id = QStringLiteral("qam-item:%1").arg(fallbackIdCounter++);
    }

    if (action->isSeparator()) {
        out = MenuItem{.kind = MenuItemKind::Separator};
        out.id = id;
        return true;
    }

    QString plainText;
    int mnemonicIndex = -1;
    extractMnemonic(action->text(), plainText, mnemonicIndex);

    out = MenuItem{};
    out.id = id;
    out.text = plainText;
    out.mnemonicIndex = mnemonicIndex;
    out.shortcutText = action->shortcut().toString(QKeySequence::NativeText);
    out.enabled = action->isEnabled();
    out.visible = action->isVisible();
    out.checkable = action->isCheckable();
    out.checked = action->isCheckable() && action->isChecked();
    out.radioGroup = radioGroupIdFor(action);

    if (!submenu) {
        out.kind = MenuItemKind::Action;
        return true;
    }

    out.kind = MenuItemKind::Submenu;
    // AGENT-GUARD: revisiting a QMenu means the (public-API constructible)
    // submenu graph contains a cycle; recursing would loop forever, and
    // stopping at the revisit would publish a truncated prefix. Fail the
    // whole snapshot instead.
    if (state.visitedMenus.contains(submenu)) {
        state.defectCode = QLatin1String(kDefectSubmenuCycle);
        return false;
    }
    state.visitedMenus.insert(submenu);

    const QList<QAction *> childActions = submenu->actions();
    if (childActions.size() > Protocol::kMaxChildrenPerItem) {
        state.defectCode = QLatin1String(kDefectTooManyChildren);
        return false;
    }
    for (const QAction *child : childActions) {
        MenuItem childItem;
        if (!buildItem(child, depth + 1, fallbackIdCounter, state, childItem)) {
            return false;
        }
        out.children.append(childItem);
    }
    state.visitedMenus.remove(submenu);
    return true;
}

} // namespace

QMenuBarMenuSource::QMenuBarMenuSource(QMenuBar *menuBar, QUuid ownerWindowId)
    : m_menuBar(menuBar)
    , m_ownerWindowId(ownerWindowId)
{
}

Exporter::MenuSnapshot QMenuBarMenuSource::snapshot() const
{
    Exporter::MenuSnapshot snapshot;
    snapshot.tree.ownerWindowId = m_ownerWindowId;

    if (!m_menuBar) {
        // AGENT-GUARD: a destroyed or not-yet-observable menu bar must never
        // read as a complete EMPTY application menu; the exporter would
        // otherwise replace its last good tree with authoritative emptiness.
        snapshot.complete = false;
        snapshot.defectCode = QLatin1String(kDefectSourceDestroyed);
        return snapshot;
    }

    WalkState state;
    int fallbackIdCounter = 0;
    const QList<QAction *> topLevel = m_menuBar->actions();
    if (topLevel.size() > Protocol::kMaxChildrenPerItem) {
        snapshot.complete = false;
        snapshot.defectCode = QLatin1String(kDefectTooManyChildren);
        return snapshot;
    }
    for (const QAction *action : topLevel) {
        MenuItem item;
        if (!buildItem(action, /*depth=*/1, fallbackIdCounter, state, item)) {
            snapshot.complete = false;
            snapshot.defectCode = state.defectCode;
            snapshot.tree.items.clear();
            return snapshot;
        }
        snapshot.tree.items.append(item);
    }
    return snapshot;
}

} // namespace QindaQt::Shell::GlobalMenu::QtWidgetsAdapter
