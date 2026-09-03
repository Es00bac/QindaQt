// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/logind_session_collaborator.h>

#include "upstream_dbus_util.h"

#include <qindaqt/services/power_protocol/power_limits.h>

#include <QtCore/QTimer>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusServiceWatcher>

namespace QindaQt::Power::Upstream {
namespace {

constexpr char kLogindServiceName[] = "org.freedesktop.login1";
constexpr char kLogindObjectPath[] = "/org/freedesktop/login1";
constexpr char kLogindManagerInterface[] = "org.freedesktop.login1.Manager";
constexpr char kPropertiesInterface[] = "org.freedesktop.DBus.Properties";

// Reads one a(ssssuu) ListInhibitors reply into the sanitized four-field
// value. UID and PID cannot survive this conversion, which is the structural
// privacy rule from Power1 v1.
bool readInhibitorList(const QVariant &value, QList<Inhibitor> &inhibitors)
{
    if (!value.canConvert<QDBusArgument>()) {
        return false;
    }
    const QDBusArgument array = value.value<QDBusArgument>();
    if (array.currentType() != QDBusArgument::ArrayType) {
        return false;
    }
    QList<Inhibitor> parsed;
    array.beginArray();
    while (!array.atEnd()) {
        QString what;
        QString who;
        QString why;
        QString mode;
        quint32 uid = 0;
        quint32 pid = 0;
        array.beginStructure();
        array >> what >> who >> why >> mode >> uid >> pid;
        array.endStructure();
        Q_UNUSED(uid)
        Q_UNUSED(pid)
        if (what.isEmpty() || mode.isEmpty()) {
            return false;
        }
        parsed.push_back(Inhibitor{.what = what,
                                   .who = who,
                                   .why = why,
                                   .mode = mode});
    }
    array.endArray();
    inhibitors = std::move(parsed);
    return true;
}

} // namespace

LogindSessionCollaborator::LogindSessionCollaborator(
    const QDBusConnection &upstreamConnection, QObject *parent)
    : SessionCollaborator(parent)
    , m_connection(upstreamConnection)
{
}

LogindSessionCollaborator::~LogindSessionCollaborator()
{
    stop();
}

quint64 LogindSessionCollaborator::start()
{
    ++m_nextGeneration;
    if (m_nextGeneration == 0) {
        ++m_nextGeneration;
    }
    m_generation = m_nextGeneration;
    m_running = true;
    m_inhibitors.clear();
    m_lidClosed = false;
    m_docked = false;
    m_preparingForSleep = false;
    m_lidProven = false;
    m_pendingReads = 0;

    if (!m_connection.isConnected()) {
        scheduleUnavailable(m_generation, QStringLiteral("logind-bus-unavailable"));
        return m_generation;
    }
    if (m_watcher == nullptr) {
        m_watcher = new QDBusServiceWatcher(
            QString::fromLatin1(kLogindServiceName), m_connection,
            QDBusServiceWatcher::WatchForOwnerChange, this);
        connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
                &LogindSessionCollaborator::onLogindOwnerChanged);
    }
    const bool sleepSignal = m_connection.connect(
        QString::fromLatin1(kLogindServiceName),
        QString::fromLatin1(kLogindObjectPath),
        QString::fromLatin1(kLogindManagerInterface),
        QStringLiteral("PrepareForSleep"), this,
        SLOT(onPrepareForSleep(bool)));
    const bool propertiesChanged = m_connection.connect(
        QString::fromLatin1(kLogindServiceName),
        QString::fromLatin1(kLogindObjectPath),
        QString::fromLatin1(kPropertiesInterface),
        QStringLiteral("PropertiesChanged"), this,
        SLOT(onLogindPropertiesChanged(const QDBusMessage&)));
    if (!sleepSignal || !propertiesChanged) {
        scheduleUnavailable(m_generation, QStringLiteral("logind-unavailable"));
        return m_generation;
    }
    refreshSession(m_generation);
    return m_generation;
}

void LogindSessionCollaborator::stop()
{
    // Signal hooks stay registered for this object's lifetime; every path is
    // generation-guarded and QtDBus drops the hooks at destruction.
    m_running = false;
    m_pendingReads = 0;
}

bool LogindSessionCollaborator::runningGeneration(const quint64 generation) const
{
    return m_running && generation != 0 && generation == m_generation;
}

void LogindSessionCollaborator::scheduleUnavailable(const quint64 generation,
                                                    const QString &reasonCode)
{
    QTimer::singleShot(0, this, [this, generation, reasonCode]() {
        if (!runningGeneration(generation)) {
            return;
        }
        Q_EMIT statusUnavailable(generation, reasonCode);
    });
}

void LogindSessionCollaborator::refreshSession(const quint64 generation)
{
    readProperties(generation);
    readInhibitors(generation);
}

void LogindSessionCollaborator::readProperties(const quint64 generation)
{
    ++m_pendingReads;
    Upstream::getAllProperties(
        m_connection, QString::fromLatin1(kLogindServiceName),
        QString::fromLatin1(kLogindObjectPath),
        QString::fromLatin1(kLogindManagerInterface), this,
        [this, generation](const QVariantMap &properties, const QString &) {
            if (!runningGeneration(generation)) {
                return;
            }
            bool lidClosed = false;
            if (Upstream::optionalBool(properties, QStringLiteral("LidClosed"),
                                       lidClosed)) {
                m_lidClosed = lidClosed;
            }
            bool docked = false;
            if (Upstream::optionalBool(properties, QStringLiteral("Docked"),
                                       docked)) {
                m_docked = docked;
            }
            bool preparingForSleep = false;
            if (Upstream::optionalBool(properties,
                                       QStringLiteral("PreparingForSleep"),
                                       preparingForSleep)) {
                m_preparingForSleep = preparingForSleep;
            }
            --m_pendingReads;
            if (m_pendingReads == 0) {
                publishFacts(generation);
            }
        },
        [this, generation](const QString &) {
            if (!runningGeneration(generation)) {
                return;
            }
            --m_pendingReads;
            scheduleUnavailable(generation, QStringLiteral("logind-unavailable"));
        });
}

void LogindSessionCollaborator::readInhibitors(const quint64 generation)
{
    ++m_pendingReads;
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kLogindServiceName),
        QString::fromLatin1(kLogindObjectPath),
        QString::fromLatin1(kLogindManagerInterface), QStringLiteral("ListInhibitors"));
    auto *watcher =
        new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation]() {
                watcher->deleteLater();
                if (!runningGeneration(generation)) {
                    return;
                }
                const QDBusMessage reply = watcher->reply();
                --m_pendingReads;
                if (reply.type() != QDBusMessage::ReplyMessage
                    || reply.arguments().isEmpty()) {
                    scheduleUnavailable(generation,
                                        QStringLiteral("logind-unavailable"));
                    return;
                }
                QList<Inhibitor> inhibitors;
                if (!readInhibitorList(reply.arguments().constFirst(),
                                       inhibitors)) {
                    scheduleUnavailable(generation,
                                        QStringLiteral("logind-malformed"));
                    return;
                }
                m_inhibitors = std::move(inhibitors);
                if (m_pendingReads == 0) {
                    publishFacts(generation);
                }
            });
}

void LogindSessionCollaborator::publishFacts(const quint64 generation)
{
    if (!runningGeneration(generation)) {
        return;
    }
    SessionFacts facts;
    if (m_lidClosed) {
        m_lidProven = true;
    }
    facts.lidPresent = m_lidProven;
    facts.lidClosed = m_lidClosed;
    facts.docked = m_docked;
    facts.preparingForSleep = m_preparingForSleep;
    facts.inhibitors = m_inhibitors;
    Q_EMIT factsChanged(generation, facts);
}

void LogindSessionCollaborator::onLogindPropertiesChanged(
    const QDBusMessage &message)
{
    if (message.arguments().size() < 2
        || message.arguments().at(0).toString()
               != QString::fromLatin1(kLogindManagerInterface)) {
        return;
    }
    const QVariantMap changed = message.arguments().at(1).toMap();
    bool touched = false;
    bool lidClosed = false;
    if (Upstream::optionalBool(changed, QStringLiteral("LidClosed"), lidClosed)) {
        m_lidClosed = lidClosed;
        touched = true;
    }
    bool docked = false;
    if (Upstream::optionalBool(changed, QStringLiteral("Docked"), docked)) {
        m_docked = docked;
        touched = true;
    }
    if (touched && runningGeneration(m_generation)) {
        publishFacts(m_generation);
    }
}

void LogindSessionCollaborator::onPrepareForSleep(const bool start)
{
    if (!runningGeneration(m_generation)) {
        return;
    }
    m_preparingForSleep = start;
    publishFacts(m_generation);
    if (!start) {
        // Inhibitor sets legitimately change across sleep; re-read rather than
        // publishing a stale summary.
        readInhibitors(m_generation);
    }
}

void LogindSessionCollaborator::onLogindOwnerChanged(const QString &name,
                                                     const QString &oldOwner,
                                                     const QString &newOwner)
{
    Q_UNUSED(name)
    if (!m_running) {
        return;
    }
    if (newOwner.isEmpty()) {
        scheduleUnavailable(m_generation, QStringLiteral("logind-unavailable"));
        return;
    }
    if (!oldOwner.isEmpty() && oldOwner != newOwner) {
        Q_EMIT authorityReplaced(m_generation);
    }
    m_lidProven = false;
    refreshSession(m_generation);
}

} // namespace QindaQt::Power::Upstream
