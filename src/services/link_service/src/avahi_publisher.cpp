// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_service/avahi_publisher.h>

#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>

namespace QindaQt::Link {
namespace {
constexpr auto kAvahi = "org.freedesktop.Avahi";
constexpr auto kServer = "org.freedesktop.Avahi.Server";
constexpr auto kGroup = "org.freedesktop.Avahi.EntryGroup";
constexpr int kAvahiUnspec = -1;
enum GroupState { Uncommitted = 0, Registering = 1, Established = 2, Collision = 3, Failure = 4 };
}

AvahiPublisher::AvahiPublisher(QDBusConnection bus, QObject *parent)
    : ServicePublisher(parent), m_bus(std::move(bus)),
      m_watcher(QString::fromLatin1(kAvahi), m_bus, QDBusServiceWatcher::WatchForOwnerChange)
{
    // A daemon restart drops our group; recreate it from the remembered advert.
    connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &newOwner) {
                m_groupPath.clear();
                ++m_generation;
                if (!m_wanted) return;
                if (newOwner.isEmpty()) setState(QStringLiteral("unavailable"));
                else attempt();
            });
}

AvahiPublisher::~AvahiPublisher() { freeGroup(); }

void AvahiPublisher::setState(const QString &state)
{
    if (state == m_state) return;
    m_state = state;
    Q_EMIT stateChanged(state);
}

void AvahiPublisher::publish(const ServiceAdvert &advert)
{
    freeGroup();
    m_advert = advert;
    m_activeName = advert.name;
    m_wanted = true;
    attempt();
}

void AvahiPublisher::withdraw()
{
    m_wanted = false;
    ++m_generation;
    freeGroup();   // Free is synchronous so the record is gone from the wire when we return
    setState(QStringLiteral("off"));
}

void AvahiPublisher::freeGroup()
{
    if (m_groupPath.isEmpty()) return;
    QDBusMessage free = QDBusMessage::createMethodCall(QString::fromLatin1(kAvahi), m_groupPath,
                                                       QString::fromLatin1(kGroup), QStringLiteral("Free"));
    m_bus.call(free, QDBus::Block, 2000);
    m_bus.disconnect(QString::fromLatin1(kAvahi), m_groupPath, QString::fromLatin1(kGroup),
                     QStringLiteral("StateChanged"), this, SLOT(onGroupState(int, QString)));
    m_groupPath.clear();
}

void AvahiPublisher::attempt()
{
    if (!m_bus.isConnected() || !m_bus.interface()->isServiceRegistered(QString::fromLatin1(kAvahi))) {
        setState(QStringLiteral("unavailable"));
        return;
    }
    const quint64 generation = ++m_generation;
    QDBusMessage call = QDBusMessage::createMethodCall(QString::fromLatin1(kAvahi), QStringLiteral("/"),
                                                       QString::fromLatin1(kServer), QStringLiteral("EntryGroupNew"));
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        if (generation != m_generation || !m_wanted) return;
        const QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError()) {
            setState(QStringLiteral("failed"));
            return;
        }
        m_groupPath = reply.value().path();
        m_bus.connect(QString::fromLatin1(kAvahi), m_groupPath, QString::fromLatin1(kGroup),
                      QStringLiteral("StateChanged"), this, SLOT(onGroupState(int, QString)));
        addAndCommit();
    });
}

void AvahiPublisher::addAndCommit()
{
    // AGENT-GUARD: build the aay by hand. QList<QByteArray> is not a registered D-Bus type, and the
    // marshalling failure only shows as a dropped message, so Avahi never receives the service.
    QDBusArgument txt;
    txt.beginArray(QMetaType::QByteArray);
    for (auto it = m_advert.txt.cbegin(); it != m_advert.txt.cend(); ++it) {
        txt << (it.key() + QLatin1Char('=') + it.value()).toUtf8();
    }
    txt.endArray();
    QDBusMessage add = QDBusMessage::createMethodCall(QString::fromLatin1(kAvahi), m_groupPath,
                                                      QString::fromLatin1(kGroup), QStringLiteral("AddService"));
    // (interface i, protocol i, flags u, name s, type s, domain s, host s, port q, txt aay)
    // AGENT-GUARD: wrap the port in QVariant::fromValue; a bare quint16 converts to int32 ('i') and
    // Avahi rejects the call with a signature mismatch.
    add << kAvahiUnspec << kAvahiUnspec << quint32(0) << m_activeName << m_advert.type << QString() << QString()
        << QVariant::fromValue(quint16(m_advert.port)) << QVariant::fromValue(txt);
    const QDBusMessage added = m_bus.call(add, QDBus::Block, 5000);
    if (added.type() == QDBusMessage::ErrorMessage) {
        setState(QStringLiteral("failed"));
        return;
    }
    QDBusMessage commit = QDBusMessage::createMethodCall(QString::fromLatin1(kAvahi), m_groupPath,
                                                         QString::fromLatin1(kGroup), QStringLiteral("Commit"));
    if (m_bus.call(commit, QDBus::Block, 5000).type() == QDBusMessage::ErrorMessage) {
        setState(QStringLiteral("failed"));
    }
}

void AvahiPublisher::onGroupState(int state, const QString &)
{
    if (!m_wanted) return;
    switch (state) {
    case Established:
        setState(m_activeName == m_advert.name ? QStringLiteral("published") : QStringLiteral("renamed"));
        break;
    case Collision: {
        QDBusMessage alt = QDBusMessage::createMethodCall(QString::fromLatin1(kAvahi), QStringLiteral("/"),
                                                          QString::fromLatin1(kServer),
                                                          QStringLiteral("GetAlternativeServiceName"));
        alt << m_activeName;
        const QDBusMessage reply = m_bus.call(alt, QDBus::Block, 5000);
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            setState(QStringLiteral("failed"));
            break;
        }
        m_activeName = reply.arguments().first().toString();
        QDBusMessage reset = QDBusMessage::createMethodCall(QString::fromLatin1(kAvahi), m_groupPath,
                                                            QString::fromLatin1(kGroup), QStringLiteral("Reset"));
        m_bus.call(reset, QDBus::Block, 5000);
        addAndCommit();
        break;
    }
    case Failure:
        setState(QStringLiteral("failed"));
        break;
    default:
        break;
    }
}

} // namespace QindaQt::Link
