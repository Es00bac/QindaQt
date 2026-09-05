// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>

#include <qindaqt/shell/global_menu/protocol/menu_item_lookup.h>
#include <qindaqt/shell/global_menu/protocol/menu_validation.h>

#include <QtCore/QVariantMap>

#include <limits>
#include <utility>

namespace QindaQt::Shell::GlobalMenu
{

namespace
{

QVariantMap projectItem(const Protocol::MenuItem &item, const QString &generation)
{
    QVariantList children;
    children.reserve(item.children.size());
    for (const Protocol::MenuItem &child : item.children) {
        if (!child.visible) {
            continue;
        }
        children.append(projectItem(child, generation));
    }
    QString kind = QStringLiteral("action");
    if (item.kind == Protocol::MenuItemKind::Submenu) {
        kind = QStringLiteral("submenu");
    } else if (item.kind == Protocol::MenuItemKind::Separator) {
        kind = QStringLiteral("separator");
    }
    return {{QStringLiteral("generation"), generation},
            {QStringLiteral("id"), item.id},
            {QStringLiteral("kind"), kind},
            {QStringLiteral("text"), item.text},
            {QStringLiteral("mnemonicIndex"), item.mnemonicIndex},
            {QStringLiteral("shortcutText"), item.shortcutText},
            {QStringLiteral("enabled"), item.enabled},
            {QStringLiteral("checkable"), item.checkable},
            {QStringLiteral("checked"), item.checked},
            {QStringLiteral("children"), children}};
}

QVariantList projectTopLevel(const Protocol::MenuTree &tree, const QString &generation)
{
    QVariantList projection;
    projection.reserve(tree.items.size());
    for (const Protocol::MenuItem &item : tree.items) {
        // Hidden entries are not presented anywhere in G0, so they are
        // omitted rather than rendered as disabled ghosts. Separators carry
        // no activation or label and are likewise not projected.
        if (item.kind == Protocol::MenuItemKind::Separator || !item.visible) {
            continue;
        }
        projection.append(projectItem(item, generation));
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

QString GlobalMenuAppletAccess::phase() const
{
    return m_phase;
}

QString GlobalMenuAppletAccess::reasonCode() const
{
    return m_reasonCode;
}

bool GlobalMenuAppletAccess::rendererPresent() const noexcept
{
    return m_rendererCount > 0;
}

void GlobalMenuAppletAccess::attachRenderer()
{
    const bool wasPresent = rendererPresent();
    if (m_rendererCount != std::numeric_limits<quint32>::max()) {
        ++m_rendererCount;
    }
    if (!wasPresent && rendererPresent()) {
        Q_EMIT rendererPresentChanged();
    }
}

void GlobalMenuAppletAccess::detachRenderer()
{
    if (m_rendererCount == 0) {
        return;
    }
    --m_rendererCount;
    if (m_rendererCount == 0) {
        Q_EMIT rendererPresentChanged();
    }
}

void GlobalMenuAppletAccess::activate(const QString &actionId, const QString &generation)
{
    if (generation != QString::number(m_generation))
        return;
    activate(actionId);
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
    setTopLevelProjection(projectTopLevel(tree, QString::number(++m_generation)));
    setAvailable(true);
    setPhase(QStringLiteral("ready"), {});
}

void GlobalMenuAppletAccess::publishUnavailable()
{
    m_tree = Protocol::MenuTree{};
    setTopLevelProjection({});
    setAvailable(false);
    setPhase(QStringLiteral("unavailable"), {});
}

void GlobalMenuAppletAccess::publishDegraded(const QString &reasonCode)
{
    m_tree = Protocol::MenuTree{};
    setTopLevelProjection({});
    setAvailable(false);
    setPhase(QStringLiteral("degraded"), reasonCode);
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

void GlobalMenuAppletAccess::setPhase(QString phase, QString reasonCode)
{
    if (m_phase == phase && m_reasonCode == reasonCode) {
        return;
    }
    m_phase = std::move(phase);
    m_reasonCode = std::move(reasonCode);
    Q_EMIT phaseChanged();
}

} // namespace QindaQt::Shell::GlobalMenu
