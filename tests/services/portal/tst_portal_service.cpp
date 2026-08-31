// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/portal/appearance_source.h"
#include "qindaqt/services/portal/resident_portal_service.h"

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusVariant>
#include <QEventLoop>
#include <QMap>
#include <QProcess>
#include <QUuid>
#include <QtTest>

#include <optional>

using namespace QindaQt::Services::Portal;

using PortalNamespaceMap = QMap<QString, QVariantMap>;
Q_DECLARE_METATYPE(PortalNamespaceMap)

class FakeAppearanceSource final : public AppearanceSource {
    Q_OBJECT
public:
    bool start(QString *error) override
    {
        ++starts;
        if (!startSucceeds) {
            if (error != nullptr) {
                *error = QStringLiteral("source rejected startup");
            }
            return false;
        }
        started = true;
        return true;
    }
    void stop() override
    {
        ++stops;
        started = false;
        truth.reset();
    }
    const std::optional<AppearanceTruth> &current() const override
    {
        return truth;
    }
    QString diagnostic() const override { return {}; }

    void publish(AppearanceTruth next)
    {
        truth = std::move(next);
        Q_EMIT currentChanged();
    }
    void withdraw()
    {
        truth.reset();
        Q_EMIT currentChanged();
    }

    std::optional<AppearanceTruth> truth;
    int starts = 0;
    int stops = 0;
    bool startSucceeds = true;
    bool started = false;
};

class ChangeReceiver final : public QObject {
    Q_OBJECT
public Q_SLOTS:
    void receive(const QString &namespaceName, const QString &key,
                 const QDBusVariant &value)
    {
        changes.append({namespaceName, key, value.variant()});
    }
public:
    struct Change final {
        QString namespaceName;
        QString key;
        QVariant value;
    };
    QList<Change> changes;
};

namespace {

class PrivateBus final {
public:
    ~PrivateBus()
    {
        if (!serviceName.isEmpty()) {
            QDBusConnection::disconnectFromBus(serviceName);
        }
        if (!clientName.isEmpty()) {
            QDBusConnection::disconnectFromBus(clientName);
        }
        daemon.terminate();
        if (!daemon.waitForFinished(2'000)) {
            daemon.kill();
            daemon.waitForFinished(2'000);
        }
    }

    bool start()
    {
        daemon.start(QStringLiteral(QINDAQT_PORTAL_DBUS_DAEMON_EXECUTABLE),
                     {QStringLiteral("--session"), QStringLiteral("--nofork"),
                      QStringLiteral("--nopidfile"),
                      QStringLiteral("--print-address=1")});
        if (!daemon.waitForStarted(5'000) || !daemon.waitForReadyRead(5'000)) {
            return false;
        }
        address = QString::fromUtf8(daemon.readLine()).trimmed();
        const QString suffix = QUuid::createUuid().toString(QUuid::Id128);
        serviceName = QStringLiteral("qindaqt-portal-service-%1").arg(suffix);
        clientName = QStringLiteral("qindaqt-portal-client-%1").arg(suffix);
        service = QDBusConnection::connectToBus(address, serviceName);
        client = QDBusConnection::connectToBus(address, clientName);
        return !address.isEmpty() && service.isConnected() && client.isConnected();
    }

    QProcess daemon;
    QString address;
    QString serviceName;
    QString clientName;
    QDBusConnection service{QStringLiteral("invalid-service")};
    QDBusConnection client{QStringLiteral("invalid-client")};
};

AppearanceTruth truth(AppearancePolicy policy, QString owner = QStringLiteral(":1.90"),
                      QString epoch = QStringLiteral("epoch"), quint64 revision = 1)
{
    return {.policy = policy,
            .owner = std::move(owner),
            .epoch = std::move(epoch),
            .revision = revision};
}

QDBusMessage waitForReply(const QDBusConnection &connection,
                          QDBusMessage message)
{
    QDBusPendingCallWatcher watcher(connection.asyncCall(message, 5'000));
    if (!watcher.isFinished()) {
        QEventLoop loop;
        QObject::connect(&watcher, &QDBusPendingCallWatcher::finished,
                         &loop, &QEventLoop::quit);
        loop.exec();
    }
    return watcher.reply();
}

QDBusMessage call(const QDBusConnection &connection, const QString &service,
                  const QString &member, const QVariantList &arguments = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        service, QString::fromLatin1(kPortalObjectPath),
        QString::fromLatin1(kPortalSettingsInterface), member);
    message.setArguments(arguments);
    return waitForReply(connection, message);
}

} // namespace

class PortalServiceTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void exportsExactStandardContractAndFailClosedTruth();
    void ownsRollsBackAndReleasesItsBackendName();
};

void PortalServiceTests::exportsExactStandardContractAndFailClosedTruth()
{
    qDBusRegisterMetaType<PortalNamespaceMap>();
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeAppearanceSource source;
    const QString serviceName = QStringLiteral(
        "org.freedesktop.impl.portal.desktop.qindaqt.Test%1")
                                    .arg(QCoreApplication::applicationPid());
    ResidentPortalService service(source, bus.service, serviceName);
    QString error;
    QCOMPARE(service.start(&error), PortalServiceStartStatus::Started);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(service.isRunning());

    // Before an exact Settings1 baseline, ReadAll is empty and Read fails.
    QDBusMessage empty = call(bus.client, serviceName, QStringLiteral("ReadAll"),
                              {QStringList{}});
    QCOMPARE(empty.type(), QDBusMessage::ReplyMessage);
    const PortalNamespaceMap emptyMap =
        qdbus_cast<PortalNamespaceMap>(empty.arguments().value(0));
    QVERIFY(emptyMap.isEmpty());
    QCOMPARE(call(bus.client, serviceName, QStringLiteral("Read"),
                  {QString::fromLatin1(kAppearanceNamespace),
                   QString::fromLatin1(kColorSchemeKey)})
                 .type(),
             QDBusMessage::ErrorMessage);

    ChangeReceiver receiver;
    QVERIFY(bus.client.connect(
        serviceName, QString::fromLatin1(kPortalObjectPath),
        QString::fromLatin1(kPortalSettingsInterface),
        QStringLiteral("SettingChanged"), &receiver,
        SLOT(receive(QString,QString,QDBusVariant))));
    source.publish(truth({.colorScheme = PortalColorScheme::PreferDark,
                          .accentColor = {.red = 0.25,
                                          .green = 0.5,
                                          .blue = 0.75},
                          .contrast = PortalContrast::PreferHigh}));
    QTRY_COMPARE(receiver.changes.size(), 3);

    QDBusMessage all = call(
        bus.client, serviceName, QStringLiteral("ReadAll"),
        {QStringList{QStringLiteral("org.freedesktop.*")}});
    QCOMPARE(all.type(), QDBusMessage::ReplyMessage);
    const PortalNamespaceMap map =
        qdbus_cast<PortalNamespaceMap>(all.arguments().value(0));
    QCOMPARE(map.size(), 1);
    const QVariantMap appearance =
        map.value(QString::fromLatin1(kAppearanceNamespace));
    QCOMPARE(appearance.size(), 3);
    QCOMPARE(appearance.value(QString::fromLatin1(kColorSchemeKey)).toUInt(),
             quint32(1));
    QCOMPARE(appearance.value(QString::fromLatin1(kContrastKey)).toUInt(),
             quint32(1));
    const QVariant accentVariant =
        appearance.value(QString::fromLatin1(kAccentColorKey));
    QCOMPARE(accentVariant.metaType(), QMetaType::fromType<QDBusArgument>());
    const QDBusArgument accentArgument = qvariant_cast<QDBusArgument>(accentVariant);
    QCOMPARE(accentArgument.currentSignature(), QStringLiteral("(ddd)"));

    QDBusMessage one = call(
        bus.client, serviceName, QStringLiteral("Read"),
        {QString::fromLatin1(kAppearanceNamespace),
         QString::fromLatin1(kColorSchemeKey)});
    QCOMPARE(one.type(), QDBusMessage::ReplyMessage);
    QCOMPARE(qvariant_cast<QDBusVariant>(one.arguments().value(0)).variant().toUInt(),
             quint32(1));

    const QDBusMessage noMatch = call(
        bus.client, serviceName, QStringLiteral("ReadAll"),
        {QStringList{QStringLiteral("org.example.*")}});
    QVERIFY(qdbus_cast<PortalNamespaceMap>(noMatch.arguments().value(0)).isEmpty());
    QCOMPARE(call(bus.client, serviceName, QStringLiteral("Read"),
                  {QStringLiteral("org.example"), QStringLiteral("missing")})
                 .type(),
             QDBusMessage::ErrorMessage);

    QStringList tooMany(65, QStringLiteral("org.freedesktop.appearance"));
    QCOMPARE(call(bus.client, serviceName, QStringLiteral("ReadAll"), {tooMany})
                 .type(),
             QDBusMessage::ErrorMessage);
    const QString oversized(257, QLatin1Char('a'));
    QCOMPARE(call(bus.client, serviceName, QStringLiteral("ReadAll"),
                  {QStringList{oversized}})
                 .type(),
             QDBusMessage::ErrorMessage);
    QString embeddedNull = QStringLiteral("color");
    embeddedNull.append(QChar::Null);
    embeddedNull.append(QStringLiteral("scheme"));
    QCOMPARE(call(bus.client, serviceName, QStringLiteral("Read"),
                  {QString::fromLatin1(kAppearanceNamespace), embeddedNull})
                 .type(),
             QDBusMessage::ErrorMessage);
    QCOMPARE(call(bus.client, serviceName, QStringLiteral("ReadAll"),
                  {QStringLiteral("wrong-signature")})
                 .type(),
             QDBusMessage::ErrorMessage);

    // Equal replacement is silent; one changed policy emits only changed keys.
    source.withdraw();
    source.publish(truth({.colorScheme = PortalColorScheme::PreferDark,
                          .accentColor = {.red = 0.25,
                                          .green = 0.5,
                                          .blue = 0.75},
                          .contrast = PortalContrast::PreferHigh},
                         QStringLiteral(":1.91"), QStringLiteral("next"), 0));
    QTest::qWait(20);
    QCOMPARE(receiver.changes.size(), 3);
    source.publish(truth({.colorScheme = PortalColorScheme::PreferLight,
                          .accentColor = {.red = 0.25,
                                          .green = 0.5,
                                          .blue = 0.75},
                          .contrast = PortalContrast::NoPreference},
                         QStringLiteral(":1.91"), QStringLiteral("next"), 1));
    QTRY_COMPARE(receiver.changes.size(), 5);

    QDBusMessage introspect = QDBusMessage::createMethodCall(
        serviceName, QString::fromLatin1(kPortalObjectPath),
        QStringLiteral("org.freedesktop.DBus.Introspectable"),
        QStringLiteral("Introspect"));
    const QDBusReply<QString> xml(waitForReply(bus.client, introspect));
    QVERIFY2(xml.isValid(), qPrintable(xml.error().message()));
    QVERIFY(xml.value().contains(QStringLiteral("org.freedesktop.impl.portal.Settings")));
    QVERIFY(xml.value().contains(QStringLiteral("type=\"a{sa{sv}}\"")));
    QVERIFY(xml.value().contains(QStringLiteral("name=\"version\" type=\"u\"")));

    service.stop();
    QVERIFY(!service.isRunning());
}

void PortalServiceTests::ownsRollsBackAndReleasesItsBackendName()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    const QString serviceName = QStringLiteral(
        "org.freedesktop.impl.portal.desktop.qindaqt.Rollback%1")
                                    .arg(QCoreApplication::applicationPid());
    FakeAppearanceSource failing;
    failing.startSucceeds = false;
    ResidentPortalService first(failing, bus.service, serviceName);
    QString error;
    QCOMPARE(first.start(&error), PortalServiceStartStatus::SourceStartFailed);
    QVERIFY(!first.isRunning());
    QVERIFY(!bus.client.interface()->isServiceRegistered(serviceName));

    FakeAppearanceSource source;
    ResidentPortalService owner(source, bus.service, serviceName);
    QCOMPARE(owner.start(), PortalServiceStartStatus::Started);

    const QString secondConnectionName = bus.serviceName + QStringLiteral("-second");
    QDBusConnection secondConnection =
        QDBusConnection::connectToBus(bus.address, secondConnectionName);
    QVERIFY(secondConnection.isConnected());
    FakeAppearanceSource loserSource;
    ResidentPortalService loser(loserSource, secondConnection, serviceName);
    QCOMPARE(loser.start(), PortalServiceStartStatus::NameAlreadyOwned);
    QVERIFY(!loser.isRunning());
    QCOMPARE(loserSource.starts, 0);

    owner.stop();
    QCOMPARE(loser.start(), PortalServiceStartStatus::Started);
    loser.stop();
    QDBusConnection::disconnectFromBus(secondConnectionName);
}

QTEST_GUILESS_MAIN(PortalServiceTests)
#include "tst_portal_service.moc"
