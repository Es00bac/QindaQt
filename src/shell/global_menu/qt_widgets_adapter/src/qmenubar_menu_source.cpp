// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/qt_widgets_adapter/qmenubar_menu_source.h>

#include <qindaqt/shell/global_menu/protocol/menu_limits.h>

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

MenuItem buildSeparator()
{
    return MenuItem{.kind = MenuItemKind::Separator};
}

MenuItem buildItem(const QAction *action, int depth, int &fallbackIdCounter)
{
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
        MenuItem item = buildSeparator();
        item.id = id;
        return item;
    }

    QString plainText;
    int mnemonicIndex = -1;
    extractMnemonic(action->text(), plainText, mnemonicIndex);

    MenuItem item;
    item.id = id;
    item.text = plainText;
    item.mnemonicIndex = mnemonicIndex;
    item.shortcutText = action->shortcut().toString(QKeySequence::NativeText);
    item.enabled = action->isEnabled();
    item.visible = action->isVisible();
    item.checkable = action->isCheckable();
    item.checked = action->isCheckable() && action->isChecked();
    item.radioGroup = radioGroupIdFor(action);

    if (submenu) {
        item.kind = MenuItemKind::Submenu;
        if (depth < Protocol::kMaxDepth) {
            const QList<QAction *> childActions = submenu->actions();
            for (int index = 0;
                 index < childActions.size() && index < Protocol::kMaxChildrenPerItem; ++index) {
                item.children.append(buildItem(childActions.at(index), depth + 1, fallbackIdCounter));
            }
        }
    } else {
        item.kind = MenuItemKind::Action;
    }

    return item;
}

} // namespace

QMenuBarMenuSource::QMenuBarMenuSource(QMenuBar *menuBar, QUuid ownerWindowId)
    : m_menuBar(menuBar)
    , m_ownerWindowId(ownerWindowId)
{
}

Protocol::MenuTree QMenuBarMenuSource::snapshot() const
{
    Protocol::MenuTree tree;
    tree.ownerWindowId = m_ownerWindowId;

    if (!m_menuBar) {
        return tree;
    }

    int fallbackIdCounter = 0;
    const QList<QAction *> topLevel = m_menuBar->actions();
    for (int index = 0;
         index < topLevel.size() && index < Protocol::kMaxChildrenPerItem; ++index) {
        tree.items.append(buildItem(topLevel.at(index), /*depth=*/1, fallbackIdCounter));
    }
    return tree;
}

} // namespace QindaQt::Shell::GlobalMenu::QtWidgetsAdapter
