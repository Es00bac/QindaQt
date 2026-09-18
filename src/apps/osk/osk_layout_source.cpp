// SPDX-License-Identifier: GPL-3.0-or-later
#include "osk_layout_source.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QList>

namespace QindaQt::Apps::Osk {
namespace {

constexpr auto Service = "org.kde.keyboard";
constexpr auto Path = "/Layouts";
constexpr auto Interface = "org.kde.KeyboardLayouts";

struct LayoutNames {
    QString shortName;
    QString displayName;
    QString longName;
};

const QDBusArgument &operator>>(const QDBusArgument &argument, LayoutNames &names)
{
    argument.beginStructure();
    argument >> names.shortName >> names.displayName >> names.longName;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const LayoutNames &names)
{
    argument.beginStructure();
    argument << names.shortName << names.displayName << names.longName;
    argument.endStructure();
    return argument;
}

QDBusMessage call(const char *method)
{
    return QDBusMessage::createMethodCall(QString::fromLatin1(Service), QString::fromLatin1(Path),
                                          QString::fromLatin1(Interface), QString::fromLatin1(method));
}

} // namespace
} // namespace QindaQt::Apps::Osk

Q_DECLARE_METATYPE(QindaQt::Apps::Osk::LayoutNames)

namespace QindaQt::Apps::Osk {

OskLayoutSource::OskLayoutSource(QDBusConnection bus, QObject *parent)
    : QObject(parent), m_bus(std::move(bus))
{
    qDBusRegisterMetaType<LayoutNames>();
    qDBusRegisterMetaType<QList<LayoutNames>>();
    m_bus.connect(QString::fromLatin1(Service), QString::fromLatin1(Path), QString::fromLatin1(Interface),
                  QStringLiteral("layoutChanged"), this, SLOT(handleLayoutChanged(uint)));
    m_bus.connect(QString::fromLatin1(Service), QString::fromLatin1(Path), QString::fromLatin1(Interface),
                  QStringLiteral("layoutListChanged"), this, SLOT(handleLayoutListChanged()));
}

QString OskLayoutSource::currentLayout() const
{
    if (m_layouts.isEmpty()) {
        return QString();
    }
    return m_layouts.value(static_cast<int>(std::min<uint>(m_index, static_cast<uint>(m_layouts.size() - 1))));
}

void OskLayoutSource::refresh()
{
    fetchLayouts();
}

void OskLayoutSource::switchToNext()
{
    m_bus.asyncCall(call("switchToNextLayout"));
}

void OskLayoutSource::handleLayoutChanged(uint index)
{
    m_index = index;
    Q_EMIT currentLayoutChanged();
}

void OskLayoutSource::handleLayoutListChanged()
{
    fetchLayouts();
}

void OskLayoutSource::fetchLayouts()
{
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call("getLayoutsList")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *finished) {
        finished->deleteLater();
        const QDBusPendingReply<QList<LayoutNames>> reply = *finished;
        m_available = reply.isValid();
        m_layouts.clear();
        if (reply.isValid()) {
            const QList<LayoutNames> names = reply.value();
            for (const LayoutNames &entry : names) {
                m_layouts.append(entry.shortName);
            }
        }
        fetchIndex();
    });
}

void OskLayoutSource::fetchIndex()
{
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call("getLayout")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *finished) {
        finished->deleteLater();
        const QDBusPendingReply<uint> reply = *finished;
        m_index = reply.isValid() ? reply.value() : 0;
        Q_EMIT currentLayoutChanged();
    });
}

} // namespace QindaQt::Apps::Osk
