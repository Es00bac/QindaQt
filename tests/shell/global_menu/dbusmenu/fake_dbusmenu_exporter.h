// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_wire.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusVariant>

namespace QindaQt::Shell::GlobalMenu::Test
{

class FakeDbusMenuExporter final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.dbusmenu")
    Q_PROPERTY(quint32 Version READ version SCRIPTABLE true)
    Q_PROPERTY(QString Status READ status SCRIPTABLE true)
    Q_PROPERTY(QString TextDirection READ textDirection SCRIPTABLE true)

public:
    explicit FakeDbusMenuExporter(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    [[nodiscard]] quint32 version() const noexcept { return 4; }
    [[nodiscard]] QString status() const { return QStringLiteral("normal"); }
    [[nodiscard]] QString textDirection() const { return QStringLiteral("ltr"); }

    void setLayout(quint32 revision, DbusMenu::LayoutItem layout)
    {
        m_revision = revision;
        m_layout = std::move(layout);
    }

    void announceLayout(quint32 advertisedRevision)
    {
        Q_EMIT LayoutUpdated(advertisedRevision, 0);
    }

    [[nodiscard]] int eventCount() const noexcept { return m_eventCount; }
    [[nodiscard]] int layoutCallCount() const noexcept { return m_layoutCallCount; }
    [[nodiscard]] int groupPropertiesCallCount() const noexcept
    {
        return m_groupPropertiesCallCount;
    }
    [[nodiscard]] int aboutToShowCallCount() const noexcept { return m_aboutToShowCallCount; }
    [[nodiscard]] qint32 lastAboutToShowId() const noexcept { return m_lastAboutToShowId; }

public Q_SLOTS:
    Q_SCRIPTABLE void GetLayout(qint32, qint32, const QStringList &, quint32 &revision,
                                DbusMenu::LayoutItem &layout)
    {
        ++m_layoutCallCount;
        revision = m_revision;
        layout = m_layout;
    }

    Q_SCRIPTABLE DbusMenu::PropertyEntryList GetGroupProperties(
        const QList<qint32> &ids, const QStringList &)
    {
        ++m_groupPropertiesCallCount;
        DbusMenu::PropertyEntryList result;
        for (qint32 id : ids) {
            result.append(DbusMenu::PropertyEntry{
                .id = id, .properties = {{QStringLiteral("enabled"), true}}});
        }
        return result;
    }

    Q_SCRIPTABLE bool AboutToShow(qint32 id)
    {
        m_lastAboutToShowId = id;
        ++m_aboutToShowCallCount;
        return true;
    }

    Q_SCRIPTABLE void Event(qint32 id, const QString &eventId, const QDBusVariant &, quint32)
    {
        ++m_eventCount;
        m_lastEventId = id;
        m_lastEventName = eventId;
        Q_EMIT eventObserved();
    }

Q_SIGNALS:
    Q_SCRIPTABLE void LayoutUpdated(quint32 revision, qint32 parentId);
    Q_SCRIPTABLE void ItemsPropertiesUpdated(DbusMenu::PropertyEntryList updated,
                                             DbusMenu::RemovedPropertyEntryList removed);
    void eventObserved();

private:
    quint32 m_revision = 1;
    DbusMenu::LayoutItem m_layout;
    int m_eventCount = 0;
    int m_layoutCallCount = 0;
    int m_groupPropertiesCallCount = 0;
    int m_aboutToShowCallCount = 0;
    qint32 m_lastAboutToShowId = -1;
    qint32 m_lastEventId = 0;
    QString m_lastEventName;
};

inline DbusMenu::LayoutItem menuLayout(const QString &label = QStringLiteral("_File"))
{
    DbusMenu::LayoutItem action{
        .id = 1,
        .properties = {{QStringLiteral("label"), label},
                       {QStringLiteral("enabled"), true},
                       {QStringLiteral("visible"), true},
                       {QStringLiteral("shortcut"),
                        QVariant::fromValue(DbusMenu::ShortcutList{
                            QStringList{QStringLiteral("Control"), QStringLiteral("O")}})}},
        .children = {}};
    DbusMenu::LayoutItem submenuAction = action;
    submenuAction.id = 11;
    DbusMenu::LayoutItem submenu{
        .id = 10,
        .properties = {{QStringLiteral("label"), QStringLiteral("_Tools")},
                       {QStringLiteral("children-display"), QStringLiteral("submenu")}},
        .children = {QVariant::fromValue(submenuAction)}};
    return DbusMenu::LayoutItem{.id = 0,
                                .properties = {},
                                .children = {QVariant::fromValue(action),
                                             QVariant::fromValue(submenu)}};
}

} // namespace QindaQt::Shell::GlobalMenu::Test
