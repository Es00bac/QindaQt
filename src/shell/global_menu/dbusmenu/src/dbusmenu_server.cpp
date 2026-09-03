// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_server.h>

#include <qindaqt/shell/global_menu/protocol/menu_limits.h>
#include <qindaqt/shell/global_menu/protocol/menu_validation.h>

#include <limits>

namespace QindaQt::Shell::GlobalMenu::DbusMenu
{
namespace
{

QVariantMap selectedProperties(const QVariantMap &properties, const QStringList &names)
{
    if (names.isEmpty()) {
        return properties;
    }
    QVariantMap selected;
    for (const QString &name : names) {
        const auto property = properties.constFind(name);
        if (property != properties.cend()) {
            selected.insert(name, *property);
        }
    }
    return selected;
}

LayoutItem boundedLayout(const LayoutItem &source, qint32 depth, const QStringList &names)
{
    LayoutItem result{.id = source.id,
                      .properties = selectedProperties(source.properties, names),
                      .children = {}};
    if (depth == 0) {
        return result;
    }
    const qint32 childDepth = depth < 0 ? -1 : depth - 1;
    result.children.reserve(source.children.size());
    for (const QVariant &childValue : source.children) {
        if (childValue.canConvert<LayoutItem>()) {
            result.children.append(
                QVariant::fromValue(boundedLayout(childValue.value<LayoutItem>(), childDepth,
                                                  names)));
        }
    }
    return result;
}

std::optional<LayoutItem> findNode(const LayoutItem &node, qint32 id)
{
    if (node.id == id) {
        return node;
    }
    for (const QVariant &childValue : node.children) {
        if (!childValue.canConvert<LayoutItem>()) {
            continue;
        }
        if (const auto found = findNode(childValue.value<LayoutItem>(), id)) {
            return found;
        }
    }
    return std::nullopt;
}

void appendProperties(const LayoutItem &node, const QStringList &names,
                      PropertyEntryList &entries)
{
    entries.append(
        {.id = node.id, .properties = selectedProperties(node.properties, names)});
    for (const QVariant &childValue : node.children) {
        if (childValue.canConvert<LayoutItem>()) {
            appendProperties(childValue.value<LayoutItem>(), names, entries);
        }
    }
}

ShortcutList shortcutList(const QString &portableShortcut)
{
    ShortcutList sequences;
    for (const QString &sequence : portableShortcut.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        QStringList tokens = sequence.trimmed().split(QLatin1Char('+'), Qt::SkipEmptyParts);
        for (QString &token : tokens) {
            if (token == QStringLiteral("Ctrl")) {
                token = QStringLiteral("Control");
            } else if (token == QStringLiteral("Meta")) {
                token = QStringLiteral("Super");
            }
        }
        if (!tokens.isEmpty()) {
            sequences.append(tokens);
        }
    }
    return sequences;
}

QVariantMap propertiesFor(const Protocol::MenuItem &item)
{
    if (item.kind == Protocol::MenuItemKind::Separator) {
        return {{QStringLiteral("type"), QStringLiteral("separator")}};
    }
    QVariantMap properties{{QStringLiteral("label"), item.text},
                           {QStringLiteral("enabled"), item.enabled},
                           {QStringLiteral("visible"), item.visible}};
    if (item.kind == Protocol::MenuItemKind::Submenu) {
        properties.insert(QStringLiteral("children-display"), QStringLiteral("submenu"));
    }
    if (!item.shortcutText.isEmpty()) {
        properties.insert(QStringLiteral("shortcut"),
                          QVariant::fromValue(shortcutList(item.shortcutText)));
    }
    if (item.checkable) {
        properties.insert(QStringLiteral("toggle-type"), QStringLiteral("checkmark"));
        properties.insert(QStringLiteral("toggle-state"), item.checked ? 1 : 0);
    }
    return properties;
}

} // namespace

DbusMenuServer::DbusMenuServer(QObject *parent)
    : QObject(parent)
{
    registerDbusMenuWireTypes();
    qRegisterMetaType<LayoutItem>("QindaQt::Shell::GlobalMenu::DbusMenu::LayoutItem");
    qRegisterMetaType<PropertyEntryList>(
        "QindaQt::Shell::GlobalMenu::DbusMenu::PropertyEntryList");
    qRegisterMetaType<RemovedPropertyEntryList>(
        "QindaQt::Shell::GlobalMenu::DbusMenu::RemovedPropertyEntryList");
    qRegisterMetaType<EventEntryList>(
        "QindaQt::Shell::GlobalMenu::DbusMenu::EventEntryList");
}

quint32 DbusMenuServer::version() const noexcept { return 4; }

QString DbusMenuServer::status() const { return QStringLiteral("normal"); }

QString DbusMenuServer::textDirection() const { return QStringLiteral("ltr"); }

QStringList DbusMenuServer::iconThemePath() const { return {}; }

bool DbusMenuServer::publish(const Protocol::MenuTree &tree)
{
    if (!Protocol::validateMenuTree(tree).accepted
        || m_revision == std::numeric_limits<quint32>::max()) {
        return false;
    }
    QHash<QString, qint32> nextIds = m_transportIds;
    QHash<qint32, QString> nextActions;
    qint32 nextTransportId = m_nextTransportId;
    LayoutItem nextLayout{.id = 0, .properties = {}, .children = {}};
    nextLayout.children.reserve(tree.items.size());
    for (const Protocol::MenuItem &item : tree.items) {
        const auto encoded = encodeNode(item, nextIds, nextActions, nextTransportId);
        if (!encoded) {
            return false;
        }
        nextLayout.children.append(QVariant::fromValue(*encoded));
    }
    m_layout = std::move(nextLayout);
    m_transportIds = std::move(nextIds);
    m_actionsByTransportId = std::move(nextActions);
    m_nextTransportId = nextTransportId;
    ++m_revision;
    Q_EMIT LayoutUpdated(m_revision, 0);
    return true;
}

quint32 DbusMenuServer::revision() const noexcept { return m_revision; }

void DbusMenuServer::GetLayout(qint32 parentId, qint32 recursionDepth,
                               const QStringList &propertyNames, quint32 &revision,
                               LayoutItem &layout) const
{
    revision = m_revision;
    const auto parent = find(parentId);
    layout = parent ? boundedLayout(*parent, recursionDepth, propertyNames) : LayoutItem{};
}

PropertyEntryList DbusMenuServer::GetGroupProperties(
    const QList<qint32> &ids, const QStringList &propertyNames) const
{
    PropertyEntryList result;
    if (ids.isEmpty()) {
        // AGENT-CONTRACT: dbusmenu v4 clients use an empty ids list to request
        // every published item. The synthetic root is not application menu
        // content, so traverse its bounded children in stable layout order.
        for (const QVariant &childValue : m_layout.children) {
            if (childValue.canConvert<LayoutItem>()) {
                appendProperties(childValue.value<LayoutItem>(), propertyNames,
                                 result);
            }
        }
        return result;
    }
    result.reserve(ids.size());
    for (qint32 id : ids) {
        if (const auto item = find(id)) {
            result.append({.id = id,
                           .properties = selectedProperties(item->properties, propertyNames)});
        }
    }
    return result;
}

QDBusVariant DbusMenuServer::GetProperty(qint32 id, const QString &name) const
{
    if (const auto item = find(id)) {
        return QDBusVariant(item->properties.value(name));
    }
    return QDBusVariant(QVariant{});
}

void DbusMenuServer::Event(qint32 id, const QString &eventId, const QDBusVariant &, quint32 timestamp)
{
    if (eventId != QStringLiteral("clicked")) {
        return;
    }
    const auto action = m_actionsByTransportId.constFind(id);
    if (action == m_actionsByTransportId.cend()) {
        return;
    }
    // AGENT-GUARD: emit once for each admitted wire event. The application
    // owns the final enabled/current-action consent check and uncertain D-Bus
    // replies must never cause this server to replay an activation.
    Q_EMIT ItemActivationRequested(id, timestamp);
    Q_EMIT actionActivated(*action);
}

QList<int> DbusMenuServer::EventGroup(const EventEntryList &events)
{
    QList<int> idErrors;
    for (const EventEntry &event : events) {
        if (!find(event.id)) {
            idErrors.append(event.id);
            continue;
        }
        Event(event.id, event.eventId, event.data, event.timestamp);
    }
    return idErrors;
}

bool DbusMenuServer::AboutToShow(qint32) const { return false; }

QList<int> DbusMenuServer::AboutToShowGroup(const QList<int> &ids,
                                            QList<int> &idErrors) const
{
    idErrors.clear();
    for (qint32 id : ids) {
        if (!find(id)) {
            idErrors.append(id);
        }
    }
    return {};
}

std::optional<qint32> DbusMenuServer::transportIdFor(const QString &stableId,
                                                     QHash<QString, qint32> &ids,
                                                     qint32 &nextId) const
{
    if (const auto existing = ids.constFind(stableId); existing != ids.cend()) {
        return *existing;
    }
    if (ids.size() >= Protocol::kMaxTotalItems || nextId <= 0
        || nextId == std::numeric_limits<qint32>::max()) {
        return std::nullopt;
    }
    const qint32 assigned = nextId++;
    ids.insert(stableId, assigned);
    return assigned;
}

std::optional<LayoutItem> DbusMenuServer::encodeNode(
    const Protocol::MenuItem &item, QHash<QString, qint32> &ids,
    QHash<qint32, QString> &actions, qint32 &nextId) const
{
    const auto id = transportIdFor(item.id, ids, nextId);
    if (!id) {
        return std::nullopt;
    }
    LayoutItem encoded{.id = *id, .properties = propertiesFor(item), .children = {}};
    if (item.kind == Protocol::MenuItemKind::Action) {
        actions.insert(*id, item.id);
    }
    encoded.children.reserve(item.children.size());
    for (const Protocol::MenuItem &child : item.children) {
        const auto encodedChild = encodeNode(child, ids, actions, nextId);
        if (!encodedChild) {
            return std::nullopt;
        }
        encoded.children.append(QVariant::fromValue(*encodedChild));
    }
    return encoded;
}

std::optional<LayoutItem> DbusMenuServer::find(qint32 id) const
{
    return findNode(m_layout, id);
}

} // namespace QindaQt::Shell::GlobalMenu::DbusMenu
