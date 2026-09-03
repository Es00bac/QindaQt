// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_wire.h>
#include <qindaqt/shell/global_menu/protocol/menu_tree.h>

#include <QtCore/QHash>
#include <QtCore/QObject>

#include <optional>

namespace QindaQt::Shell::GlobalMenu::DbusMenu
{

// Complete standard dbusmenu v4 server boundary. It owns a copied wire
// snapshot and remote revision only; application action authority stays with
// the injected signal consumer. publish() validates atomically and fails
// closed at revision/id exhaustion. All calls and publication must occur on
// this object's thread.
class DbusMenuServer final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.dbusmenu")
    Q_PROPERTY(quint32 Version READ version SCRIPTABLE true)
    Q_PROPERTY(QString Status READ status SCRIPTABLE true)
    Q_PROPERTY(QString TextDirection READ textDirection SCRIPTABLE true)
    Q_PROPERTY(QStringList IconThemePath READ iconThemePath SCRIPTABLE true)

public:
    explicit DbusMenuServer(QObject *parent = nullptr);

    [[nodiscard]] quint32 version() const noexcept;
    [[nodiscard]] QString status() const;
    [[nodiscard]] QString textDirection() const;
    [[nodiscard]] QStringList iconThemePath() const;

    [[nodiscard]] bool publish(const Protocol::MenuTree &tree);
    [[nodiscard]] quint32 revision() const noexcept;

public Q_SLOTS:
    Q_SCRIPTABLE void GetLayout(qint32 parentId, qint32 recursionDepth,
                                const QStringList &propertyNames, quint32 &revision,
                                LayoutItem &layout) const;
    Q_SCRIPTABLE PropertyEntryList GetGroupProperties(
        const QList<qint32> &ids, const QStringList &propertyNames) const;
    Q_SCRIPTABLE QDBusVariant GetProperty(qint32 id, const QString &name) const;
    Q_SCRIPTABLE void Event(qint32 id, const QString &eventId,
                            const QDBusVariant &data, quint32 timestamp);
    Q_SCRIPTABLE QList<int> EventGroup(const EventEntryList &events);
    Q_SCRIPTABLE bool AboutToShow(qint32 id) const;
    Q_SCRIPTABLE QList<int> AboutToShowGroup(const QList<int> &ids,
                                             QList<int> &idErrors) const;

Q_SIGNALS:
    Q_SCRIPTABLE void LayoutUpdated(quint32 revision, qint32 parentId);
    Q_SCRIPTABLE void ItemsPropertiesUpdated(PropertyEntryList updated,
                                             RemovedPropertyEntryList removed);
    Q_SCRIPTABLE void ItemActivationRequested(qint32 id, quint32 timestamp);
    void actionActivated(QString stableActionId);

private:
    [[nodiscard]] std::optional<qint32> transportIdFor(const QString &stableId,
                                                       QHash<QString, qint32> &ids,
                                                       qint32 &nextId) const;
    [[nodiscard]] std::optional<LayoutItem> encodeNode(
        const Protocol::MenuItem &item, QHash<QString, qint32> &ids,
        QHash<qint32, QString> &actions, qint32 &nextId) const;
    [[nodiscard]] std::optional<LayoutItem> find(qint32 id) const;

    LayoutItem m_layout;
    QHash<QString, qint32> m_transportIds;
    QHash<qint32, QString> m_actionsByTransportId;
    qint32 m_nextTransportId = 1;
    quint32 m_revision = 0;
};

} // namespace QindaQt::Shell::GlobalMenu::DbusMenu
