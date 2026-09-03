// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/logind_session_collaborator.h>

#include "upstream_dbus_util.h"

#include <qindaqt/services/power_protocol/power_limits.h>

#include <QtCore/QTimer>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusReply>

namespace QindaQt::Power::Upstream {
namespace {

constexpr char kLogindServiceName[] = "org.freedesktop.login1";
constexpr char kLogindObjectPath[] = "/org/freedesktop/login1";
constexpr char kLogindManagerInterface[] = "org.freedesktop.login1.Manager";
constexpr char kPropertiesInterface[] = "org.freedesktop.DBus.Properties";

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
        parsed.push_back(Inhibitor{.what = what, .who = who, .why = why, .mode = mode});
        if (parsed.size() > kMaxInhibitors) {
            break;
        }
    }
    array.endArray();
    inhibitors = std::move(parsed);
    return true;
}

bool readOptionalBool(const QVariantMap &properties, const QString &name,
                      bool &value)
{
    const auto property = properties.constFind(name);
    if (property == properties.constEnd()) {
        return true;
    }
    if (property->metaType() != QMetaType::fromType<bool>()) {
        return false;
    }
    value = property->toBool();
    return true;
}

} // namespace

struct LogindSessionCollaborator::RefreshCycle
{
    quint64 generation = 0;
    quint64 serial = 0;
    QString owner;
    QList<Inhibitor> inhibitors;
    bool lidClosed = false;
    bool docked = false;
    bool preparingForSleep = false;
    bool propertiesDone = false;
    bool inhibitorsDone = false;
};

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
    m_refresh.reset();
    m_activeOwner.clear();
    m_lastOwner.clear();
    m_inhibitors.clear();
    m_lidClosed = false;
    m_docked = false;
    m_preparingForSleep = false;
    m_lidProven = false;
    m_epochAdvancedForLoss = false;

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
    if (!subscribeServiceSignals()) {
        scheduleUnavailable(m_generation, QStringLiteral("logind-unavailable"));
        return m_generation;
    }
    beginRefresh();
    return m_generation;
}

void LogindSessionCollaborator::stop()
{
    m_running = false;
    m_refresh.reset();
    m_activeOwner.clear();
}

bool LogindSessionCollaborator::subscribeServiceSignals()
{
    if (m_signalsSubscribed) {
        return true;
    }
    const bool sleepSignal = m_connection.connect(
        QString::fromLatin1(kLogindServiceName), QString::fromLatin1(kLogindObjectPath),
        QString::fromLatin1(kLogindManagerInterface), QStringLiteral("PrepareForSleep"),
        this, SLOT(onPrepareForSleep(bool)));
    const bool propertiesChanged = m_connection.connect(
        QString::fromLatin1(kLogindServiceName), QString::fromLatin1(kLogindObjectPath),
        QString::fromLatin1(kPropertiesInterface), QStringLiteral("PropertiesChanged"),
        this, SLOT(onLogindPropertiesChanged(const QDBusMessage&)));
    m_signalsSubscribed = sleepSignal && propertiesChanged;
    return m_signalsSubscribed;
}

bool LogindSessionCollaborator::runningGeneration(const quint64 generation) const
{
    return m_running && generation != 0 && generation == m_generation;
}

void LogindSessionCollaborator::scheduleUnavailable(const quint64 generation,
                                                    const QString &reasonCode)
{
    QTimer::singleShot(0, this, [this, generation, reasonCode]() {
        if (runningGeneration(generation)) {
            Q_EMIT statusUnavailable(generation, reasonCode);
        }
    });
}

void LogindSessionCollaborator::beginRefresh()
{
    if (!runningGeneration(m_generation)) {
        return;
    }
    auto cycle = std::make_shared<RefreshCycle>();
    cycle->generation = m_generation;
    cycle->serial = ++m_nextRefresh;
    const QDBusReply<QString> owner = m_connection.interface()->serviceOwner(
        QString::fromLatin1(kLogindServiceName));
    if (!owner.isValid() || owner.value().isEmpty()) {
        scheduleUnavailable(m_generation, QStringLiteral("logind-unavailable"));
        return;
    }
    cycle->owner = owner.value();
    cycle->lidClosed = m_lidClosed;
    cycle->docked = m_docked;
    cycle->preparingForSleep = m_preparingForSleep;
    m_refresh = cycle;
    readProperties(cycle);
    readInhibitors(cycle);
}

bool LogindSessionCollaborator::acceptReplyOwner(
    const std::shared_ptr<RefreshCycle> &cycle, const QString &owner)
{
    if (m_refresh != cycle || !runningGeneration(cycle->generation)
        || owner.isEmpty()) {
        return false;
    }
    if (cycle->owner.isEmpty()) {
        cycle->owner = owner;
    }
    if (cycle->owner != owner || (!m_activeOwner.isEmpty() && m_activeOwner != owner)) {
        failRefresh(cycle, QStringLiteral("logind-unavailable"));
        return false;
    }
    return true;
}

void LogindSessionCollaborator::readProperties(
    const std::shared_ptr<RefreshCycle> &cycle)
{
    getAllProperties(
        m_connection, QString::fromLatin1(kLogindServiceName),
        QString::fromLatin1(kLogindObjectPath),
        QString::fromLatin1(kLogindManagerInterface), this,
        [this, cycle](const QVariantMap &properties, const QString &owner) {
            if (!acceptReplyOwner(cycle, owner)) {
                return;
            }
            if (!readOptionalBool(properties, QStringLiteral("LidClosed"),
                                  cycle->lidClosed)
                || !readOptionalBool(properties, QStringLiteral("Docked"), cycle->docked)
                || !readOptionalBool(properties, QStringLiteral("PreparingForSleep"),
                                     cycle->preparingForSleep)) {
                failRefresh(cycle, QStringLiteral("logind-malformed"));
                return;
            }
            cycle->propertiesDone = true;
            tryPublish(cycle);
        },
        [this, cycle](const QString &) {
            failRefresh(cycle, QStringLiteral("logind-unavailable"));
        });
}

void LogindSessionCollaborator::readInhibitors(
    const std::shared_ptr<RefreshCycle> &cycle)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        cycle->owner, QString::fromLatin1(kLogindObjectPath),
        QString::fromLatin1(kLogindManagerInterface), QStringLiteral("ListInhibitors"));
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, cycle]() {
                const QDBusMessage reply = watcher->reply();
                watcher->deleteLater();
                if (!acceptReplyOwner(cycle, cycle->owner)) {
                    return;
                }
                if (reply.type() != QDBusMessage::ReplyMessage
                    || reply.arguments().size() != 1) {
                    failRefresh(cycle, QStringLiteral("logind-unavailable"));
                    return;
                }
                if (!readInhibitorList(reply.arguments().constFirst(),
                                       cycle->inhibitors)) {
                    failRefresh(cycle, QStringLiteral("logind-malformed"));
                    return;
                }
                cycle->inhibitorsDone = true;
                tryPublish(cycle);
            });
}

void LogindSessionCollaborator::tryPublish(
    const std::shared_ptr<RefreshCycle> &cycle)
{
    if (m_refresh != cycle || !cycle->propertiesDone || !cycle->inhibitorsDone) {
        return;
    }
    adoptOwner(cycle->owner);
    m_lidClosed = cycle->lidClosed;
    m_docked = cycle->docked;
    m_preparingForSleep = cycle->preparingForSleep;
    m_inhibitors = std::move(cycle->inhibitors);
    m_refresh.reset();
    publishFacts(cycle->generation);
}

void LogindSessionCollaborator::failRefresh(
    const std::shared_ptr<RefreshCycle> &cycle, const QString &reasonCode)
{
    if (m_refresh != cycle || !runningGeneration(cycle->generation)) {
        return;
    }
    m_refresh.reset();
    scheduleUnavailable(cycle->generation, reasonCode);
}

void LogindSessionCollaborator::adoptOwner(const QString &owner)
{
    if (owner.isEmpty() || owner == m_activeOwner) {
        return;
    }
    if (!m_lastOwner.isEmpty() && m_lastOwner != owner && !m_epochAdvancedForLoss) {
        Q_EMIT authorityReplaced(m_generation);
    }
    m_activeOwner = owner;
    m_lastOwner = owner;
    m_epochAdvancedForLoss = false;
}

void LogindSessionCollaborator::publishFacts(const quint64 generation)
{
    if (!runningGeneration(generation) || m_activeOwner.isEmpty()) {
        return;
    }
    if (m_lidClosed) {
        m_lidProven = true;
    }
    SessionFacts facts;
    facts.lidPresent = m_lidProven;
    facts.lidClosed = m_lidClosed;
    facts.docked = m_docked;
    facts.preparingForSleep = m_preparingForSleep;
    facts.inhibitors = m_inhibitors;
    Q_EMIT factsChanged(generation, facts);
}

void LogindSessionCollaborator::onLogindPropertiesChanged(const QDBusMessage &message)
{
    if (message.arguments().isEmpty()
        || message.arguments().constFirst().toString()
            != QString::fromLatin1(kLogindManagerInterface)) {
        return;
    }
    beginRefresh();
}

void LogindSessionCollaborator::onPrepareForSleep(const bool start)
{
    if (!runningGeneration(m_generation) || m_activeOwner.isEmpty()) {
        return;
    }
    m_preparingForSleep = start;
    publishFacts(m_generation);
    if (!start) {
        beginRefresh();
    }
}

void LogindSessionCollaborator::onLogindOwnerChanged(const QString &name,
                                                     const QString &oldOwner,
                                                     const QString &newOwner)
{
    Q_UNUSED(name)
    Q_UNUSED(oldOwner)
    Q_UNUSED(newOwner)
    if (!m_running) {
        return;
    }
    const QDBusReply<QString> resolved = m_connection.interface()->serviceOwner(
        QString::fromLatin1(kLogindServiceName));
    const QString currentOwner = resolved.isValid() ? resolved.value() : QString();
    if (currentOwner.isEmpty()) {
        m_refresh.reset();
        if (!m_activeOwner.isEmpty()) {
            m_lastOwner = m_activeOwner;
            m_activeOwner.clear();
            m_lidProven = false;
            Q_EMIT authorityReplaced(m_generation);
            m_epochAdvancedForLoss = true;
        }
        scheduleUnavailable(m_generation, QStringLiteral("logind-unavailable"));
        return;
    }
    if (!m_activeOwner.isEmpty() && m_activeOwner != currentOwner) {
        m_lastOwner = m_activeOwner;
        m_activeOwner.clear();
        m_lidProven = false;
        Q_EMIT authorityReplaced(m_generation);
        m_epochAdvancedForLoss = true;
        Q_EMIT statusUnavailable(m_generation,
                                 QStringLiteral("logind-unavailable"));
    }
    beginRefresh();
}

} // namespace QindaQt::Power::Upstream
