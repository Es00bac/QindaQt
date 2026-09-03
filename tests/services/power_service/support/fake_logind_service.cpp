// SPDX-License-Identifier: GPL-3.0-or-later

#include "fake_logind_service.h"


#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusMetaType>

#include <QtCore/QVariant>

namespace QindaQt::Tests {
namespace {

constexpr char kServiceName[] = "org.freedesktop.login1";
constexpr char kObjectPath[] = "/org/freedesktop/login1";
constexpr char kManagerInterface[] = "org.freedesktop.login1.Manager";
constexpr char kPropertiesInterface[] = "org.freedesktop.DBus.Properties";

QVariant inhibitorArrayValue(const QList<FakeLogindService::InhibitorSpec> &inhibitors)
{
    QList<FakeLogindInhibitorWire> values;
    for (const FakeLogindService::InhibitorSpec &inhibitor : inhibitors) {
        values.push_back({inhibitor.what, inhibitor.who, inhibitor.why,
                          inhibitor.mode, inhibitor.uid, inhibitor.pid});
    }
    return QVariant::fromValue(values);
}

QVariantMap managerPropertyMap(const bool lidClosed, const bool docked,
                               const bool preparingForSleep)
{
    QVariantMap properties;
    properties.insert(QStringLiteral("LidClosed"), QVariant(lidClosed));
    properties.insert(QStringLiteral("Docked"), QVariant(docked));
    properties.insert(QStringLiteral("PreparingForSleep"), QVariant(preparingForSleep));
    return properties;
}

} // namespace

QDBusArgument &operator<<(QDBusArgument &argument,
                          const FakeLogindInhibitorWire &value)
{
    argument.beginStructure();
    argument << value.what << value.who << value.why << value.mode << value.uid
             << value.pid;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                FakeLogindInhibitorWire &value)
{
    argument.beginStructure();
    argument >> value.what >> value.who >> value.why >> value.mode >> value.uid
        >> value.pid;
    argument.endStructure();
    return argument;
}

FakeLogindService::FakeLogindService(const QDBusConnection &connection,
                                     QObject *parent)
    : QDBusVirtualObject(parent)
    , m_connection(connection)
{
    qDBusRegisterMetaType<FakeLogindInhibitorWire>();
    qDBusRegisterMetaType<QList<FakeLogindInhibitorWire>>();
}

FakeLogindService::~FakeLogindService()
{
    unregisterService();
}

bool FakeLogindService::registerService()
{
    if (!m_connection.registerVirtualObject(QString::fromLatin1(kObjectPath), this,
                                            QDBusConnection::SubPath)) {
        return false;
    }
    if (!m_connection.registerService(QString::fromLatin1(kServiceName))) {
        m_connection.unregisterObject(QString::fromLatin1(kObjectPath));
        return false;
    }
    return true;
}

void FakeLogindService::unregisterService()
{
    m_connection.unregisterObject(QString::fromLatin1(kObjectPath));
    m_connection.unregisterService(QString::fromLatin1(kServiceName));
}

void FakeLogindService::setSessionTruth(const bool lidClosed, const bool docked,
                                        const bool preparingForSleep)
{
    m_lidClosed = lidClosed;
    m_docked = docked;
    m_preparingForSleep = preparingForSleep;
}

void FakeLogindService::setInhibitors(const QList<InhibitorSpec> &inhibitors)
{
    m_inhibitors = inhibitors;
}

void FakeLogindService::setCanAnswers(const QString &powerOff, const QString &reboot,
                                      const QString &suspend, const QString &hibernate)
{
    m_canPowerOff = powerOff;
    m_canReboot = reboot;
    m_canSuspend = suspend;
    m_canHibernate = hibernate;
}

void FakeLogindService::setFailHibernate(const bool fail)
{
    m_failHibernate = fail;
}

void FakeLogindService::setDeferNextActionReply(const bool defer)
{
    m_deferNextActionReply = defer;
}

void FakeLogindService::completeDeferredActionReply()
{
    if (m_deferredReply.type() == QDBusMessage::ReplyMessage) {
        m_connection.send(m_deferredReply);
        m_deferredReply = QDBusMessage();
    }
}

void FakeLogindService::emitPrepareForSleep(const bool start)
{
    QDBusMessage signal = QDBusMessage::createSignal(
        QString::fromLatin1(kObjectPath), QString::fromLatin1(kManagerInterface),
        QStringLiteral("PrepareForSleep"));
    signal.setArguments({QVariant(start)});
    m_connection.send(signal);
}

void FakeLogindService::emitManagerPropertiesChanged()
{
    QDBusMessage signal = QDBusMessage::createSignal(
        QString::fromLatin1(kObjectPath), QString::fromLatin1(kPropertiesInterface),
        QStringLiteral("PropertiesChanged"));
    signal.setArguments(
        {QString::fromLatin1(kManagerInterface),
         QVariant(managerPropertyMap(m_lidClosed, m_docked, m_preparingForSleep)),
         QStringList{}});
    m_connection.send(signal);
}

QString FakeLogindService::introspect(const QString &path) const
{
    Q_UNUSED(path)
    return QStringLiteral("<node/>");
}

void FakeLogindService::sendError(const QDBusMessage &message, const QString &name,
                                  const QString &text)
{
    m_connection.send(message.createErrorReply(name, text));
}

bool FakeLogindService::handleMessage(const QDBusMessage &message,
                                      const QDBusConnection &connection)
{
    Q_UNUSED(connection)
    if (message.type() != QDBusMessage::MethodCallMessage) {
        return false;
    }
    const QString member = message.member();

    if (message.interface() == QString::fromLatin1(kPropertiesInterface)) {
        if (message.arguments().size() != 1
            || message.arguments().constFirst().toString()
                   != QString::fromLatin1(kManagerInterface)
            || member != QStringLiteral("GetAll")) {
            sendError(message, QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"),
                      QStringLiteral("Bad Properties call"));
            return true;
        }
        QDBusMessage reply = message.createReply();
        reply.setArguments({QVariant(
            managerPropertyMap(m_lidClosed, m_docked, m_preparingForSleep))});
        m_connection.send(reply);
        return true;
    }
    if (message.interface() != QString::fromLatin1(kManagerInterface)) {
        sendError(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
                  QStringLiteral("Unsupported interface"));
        return true;
    }

    if (member == QStringLiteral("ListInhibitors")) {
        ++listInhibitorsCallsCount;
        QDBusMessage reply = message.createReply();
        reply.setArguments({inhibitorArrayValue(m_inhibitors)});
        m_connection.send(reply);
        return true;
    }
    if (member == QStringLiteral("CanPowerOff") || member == QStringLiteral("CanReboot")
        || member == QStringLiteral("CanSuspend")
        || member == QStringLiteral("CanHibernate")) {
        const QString answer = member == QStringLiteral("CanPowerOff") ? m_canPowerOff
            : member == QStringLiteral("CanReboot")                  ? m_canReboot
            : member == QStringLiteral("CanSuspend")                 ? m_canSuspend
                                                                     : m_canHibernate;
        QDBusMessage reply = message.createReply();
        reply.setArguments({QVariant(answer)});
        m_connection.send(reply);
        return true;
    }
    if (member == QStringLiteral("PowerOff") || member == QStringLiteral("Reboot")
        || member == QStringLiteral("Suspend") || member == QStringLiteral("Hibernate")) {
        ActionCall call;
        call.method = member;
        call.interactive =
            !message.arguments().isEmpty() && message.arguments().constFirst().toBool();
        actionCalls.push_back(call);
        if (member == QStringLiteral("Hibernate") && m_failHibernate) {
            sendError(message, QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
                      QStringLiteral("Fake hibernate failure"));
            return true;
        }
        QDBusMessage reply = message.createReply();
        if (m_deferNextActionReply) {
            m_deferNextActionReply = false;
            m_deferredReply = reply;
            return true;
        }
        m_connection.send(reply);
        return true;
    }
    sendError(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
              QStringLiteral("Unknown member ") + member);
    return true;
}

} // namespace QindaQt::Tests
