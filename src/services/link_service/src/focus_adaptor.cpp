// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_service/focus_adaptor.h>

namespace QindaQt::Link {

FocusAdaptor::FocusAdaptor(FocusStore &store, QDBusConnection bus, QObject *exported, CallerAuthority *authority)
    : QDBusAbstractAdaptor(exported), m_store(store), m_authority(authority), m_bus(std::move(bus)), m_watcher(this)
{
    m_watcher.setConnection(m_bus);
    m_watcher.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(&m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &name) {
        if (name != m_publisher) return;
        m_watcher.removeWatchedService(name);
        m_publisher.clear();
        m_store.clear();
    });
}

void FocusAdaptor::Publish(const QString &json, const QDBusMessage &msg)
{
    msg.setDelayedReply(true);
    if (m_authority == nullptr) {
        m_bus.send(msg.createErrorReply(QStringLiteral("org.qindaqt.Link1.Error.Denied"), QStringLiteral("denied")));
        return;
    }
    if (!m_authority->authorizeRole(msg, CallerRole::Shell)) return; // queued the Denied reply itself
    const QByteArray utf8 = json.toUtf8();
    if (m_store.publish(utf8) == FocusStore::Result::Rejected) {
        m_bus.send(msg.createErrorReply(QStringLiteral("org.qindaqt.Link1.Error.BadRequest"),
                                        QStringLiteral("invalid focus fact")));
        return;
    }
    const QString sender = msg.service();
    if (sender != m_publisher) {
        if (!m_publisher.isEmpty()) m_watcher.removeWatchedService(m_publisher);
        m_publisher = sender;
        m_watcher.addWatchedService(sender);
    }
    m_bus.send(msg.createReply());
}

} // namespace QindaQt::Link
