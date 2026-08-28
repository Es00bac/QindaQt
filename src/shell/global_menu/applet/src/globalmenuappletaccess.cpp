// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>

#include <qindaqt/shell/global_menu/protocol/menu_item_lookup.h>
#include <qindaqt/shell/global_menu/protocol/menu_validation.h>

#include <QtCore/QVariantMap>

#include <utility>

namespace QindaQt::Shell::GlobalMenu
{

namespace
{

QVariantList projectTopLevel(const Protocol::MenuTree &tree)
{
    QVariantList projection;
    projection.reserve(tree.items.size());
    for (const Protocol::MenuItem &item : tree.items) {
        if (item.kind == Protocol::MenuItemKind::Separator) {
            continue;
        }
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), item.id);
        entry.insert(QStringLiteral("text"), item.text);
        entry.insert(QStringLiteral("mnemonicIndex"), item.mnemonicIndex);
        entry.insert(QStringLiteral("enabled"), item.enabled && item.visible);
        entry.insert(QStringLiteral("checkable"), item.checkable);
        entry.insert(QStringLiteral("checked"), item.checked);
        projection.append(entry);
    }
    return projection;
}

} // namespace

GlobalMenuAppletAccess::GlobalMenuAppletAccess(QObject *parent)
    : QObject(parent)
{
}

bool GlobalMenuAppletAccess::available() const noexcept
{
    return m_available;
}

QVariantList GlobalMenuAppletAccess::items() const
{
    return m_topLevelProjection;
}

void GlobalMenuAppletAccess::activate(const QString &actionId)
{
    // AGENT-GUARD: this authority check is the complete boundary offered to
    // QML. A disabled/invisible/unknown/non-action id must never reach
    // `activationRequested`, mirroring NotificationCenterAppletAccess::toggle().
    if (!m_available) {
        return;
    }
    const Protocol::MenuItem *item = Protocol::findMenuItemById(m_tree.items, actionId);
    if (!item || item->kind != Protocol::MenuItemKind::Action || !item->enabled
        || !item->visible) {
        return;
    }
    Q_EMIT activationRequested(actionId);
}

void GlobalMenuAppletAccess::publishTree(const Protocol::MenuTree &tree)
{
    // AGENT-GUARD: the facade re-validates independently of MenuExporter so a
    // future composition path cannot smuggle an unbounded or malformed tree
    // past the exporter into QML. Rejection means "no menu", never a partial
    // tree: QML renders the unavailable placeholder instead.
    if (!Protocol::validateMenuTree(tree).accepted) {
        publishUnavailable();
        return;
    }
    m_tree = tree;
    setTopLevelProjection(projectTopLevel(tree));
    setAvailable(true);
}

void GlobalMenuAppletAccess::publishUnavailable()
{
    m_tree = Protocol::MenuTree{};
    setTopLevelProjection({});
    setAvailable(false);
}

void GlobalMenuAppletAccess::setAvailable(bool available)
{
    if (m_available == available) {
        return;
    }
    m_available = available;
    Q_EMIT availableChanged();
}

void GlobalMenuAppletAccess::setTopLevelProjection(QVariantList projection)
{
    if (m_topLevelProjection == projection) {
        return;
    }
    m_topLevelProjection = std::move(projection);
    Q_EMIT itemsChanged();
}

} // namespace QindaQt::Shell::GlobalMenu
