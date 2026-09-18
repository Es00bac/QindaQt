// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/obs_client/obs_secret_store.h>

#include <QDBusArgument>
#include <QDBusContext>
#include <QDBusError>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QProcess>
#include <QTest>
#include <QUuid>

using namespace QindaQt::Obs;

namespace {

// Private dbus-daemon fixture: no row here touches the user's real keyring.
class PrivateBus final {
public:
    bool start() {
        process.setProgram(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE));
        process.setArguments({QStringLiteral("--session"),
                              QStringLiteral("--nofork"),
                              QStringLiteral("--nopidfile"),
                              QStringLiteral("--print-address=1")});
        process.start();
        if (!process.waitForStarted() || !process.waitForReadyRead()) {
            return false;
        }
        address = QString::fromUtf8(process.readLine()).trimmed();
        name = QStringLiteral("qindaqt-obs-secret-test-%1")
                   .arg(QUuid::createUuid().toString(QUuid::Id128));
        connection = QDBusConnection::connectToBus(address, name);
        return !address.isEmpty() && connection.isConnected();
    }
    ~PrivateBus() {
        if (!name.isEmpty()) {
            QDBusConnection::disconnectFromBus(name);
        }
        process.terminate();
        if (!process.waitForFinished(1000)) {
            process.kill();
            process.waitForFinished();
        }
    }
    QProcess process;
    QString address;
    QString name;
    QDBusConnection connection{QStringLiteral("invalid")};
};

struct WireSecret {
    QDBusObjectPath session;
    QByteArray parameters;
    QByteArray value;
    QString contentType;
};

QDBusArgument &operator<<(QDBusArgument &argument, const WireSecret &secret) {
    argument.beginStructure();
    argument << secret.session << secret.parameters << secret.value
             << secret.contentType;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                WireSecret &secret) {
    argument.beginStructure();
    argument >> secret.session >> secret.parameters >> secret.value >>
        secret.contentType;
    argument.endStructure();
    return argument;
}

} // namespace

Q_DECLARE_METATYPE(WireSecret)

namespace {

// A miniature org.freedesktop.secrets: one collection, one item, plain
// session transfer. It records the attributes it was asked to store under so
// a row can assert them.
class FakeSecretService final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Secret.Service")
public:
    QVariantMap storedAttributes;
    QByteArray storedValue;
    QString storedLabel;
    bool hasItem = false;
    bool refuseSearch = false;

    bool publish(QDBusConnection bus) {
        m_bus = bus;
        return bus.registerService(QStringLiteral("org.freedesktop.secrets")) &&
               bus.registerObject(QStringLiteral("/org/freedesktop/secrets"),
                                  this, QDBusConnection::ExportAllContents);
    }

public Q_SLOTS:
    // AGENT-CONTRACT: these signatures are the Secret Service spec's, out
    // parameters and all. Qt puts a slot's RETURN value first in the reply,
    // so a fake that returns the path would put the arguments in the
    // opposite order to the real service and quietly let a client bug pass.
    //   SearchItems(IN a{ss}, OUT ao unlocked, OUT ao locked)
    //   OpenSession(IN s algorithm, IN v input, OUT v output, OUT o result)
    Q_SCRIPTABLE void SearchItems(const QMap<QString, QString> &attributes,
                                  QList<QDBusObjectPath> &unlocked,
                                  QList<QDBusObjectPath> &locked) {
        locked = {};
        unlocked = {};
        lastSearch = attributes;
        if (refuseSearch || !hasItem) {
            return;
        }
        unlocked = {QDBusObjectPath(
            QStringLiteral("/org/freedesktop/secrets/collection/default/1"))};
    }
    Q_SCRIPTABLE void OpenSession(const QString &algorithm, const QDBusVariant &,
                                  QDBusVariant &output,
                                  QDBusObjectPath &result) {
        lastAlgorithm = algorithm;
        output = QDBusVariant(QString());
        result =
            QDBusObjectPath(QStringLiteral("/org/freedesktop/secrets/session/1"));
    }

public:
    QMap<QString, QString> lastSearch;
    QString lastAlgorithm;

private:
    QDBusConnection m_bus{QStringLiteral("invalid")};
};

class FakeSecretItem final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Secret.Item")
public:
    QByteArray value;
    bool publish(QDBusConnection bus) {
        return bus.registerObject(
            QStringLiteral("/org/freedesktop/secrets/collection/default/1"), this,
            QDBusConnection::ExportAllContents);
    }
public Q_SLOTS:
    Q_SCRIPTABLE WireSecret GetSecret(const QDBusObjectPath &session) {
        WireSecret secret;
        secret.session = session;
        secret.value = value;
        secret.contentType = QStringLiteral("text/plain");
        return secret;
    }
};

class FakeSecretCollection final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Secret.Collection")
public:
    QVariantMap lastProperties;
    QByteArray lastValue;
    bool lastReplace = false;
    int creates = 0;
    bool refuse = false;

    bool publish(QDBusConnection bus) {
        return bus.registerObject(
            QStringLiteral("/org/freedesktop/secrets/aliases/default"), this,
            QDBusConnection::ExportAllContents);
    }
public Q_SLOTS:
    //   CreateItem(IN a{sv}, IN (oayays), IN b, OUT o item, OUT o prompt)
    Q_SCRIPTABLE void CreateItem(const QVariantMap &properties,
                                 const WireSecret &secret, bool replace,
                                 QDBusObjectPath &item, QDBusObjectPath &prompt) {
        ++creates;
        lastProperties = properties;
        lastValue = secret.value;
        lastReplace = replace;
        prompt = QDBusObjectPath(QStringLiteral("/"));
        item = QDBusObjectPath(QStringLiteral("/"));
        if (refuse) {
            sendErrorReply(QDBusError::AccessDenied, QStringLiteral("locked"));
            return;
        }
        item = QDBusObjectPath(
            QStringLiteral("/org/freedesktop/secrets/collection/default/1"));
    }
};

} // namespace

class ObsSecretStoreTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void noKeyringIsAnErrorNotAnEmptyPassword();
    void anAbsentSecretIsTheOrdinaryFirstRunState();
    void storingUsesTheScopedAttributesAndReplaces();
    void aStoredSecretComesBackVerbatim();
    void aRefusedStoreIsReported();
    void anEmptyPasswordIsNeverStored();
};

void ObsSecretStoreTest::initTestCase() {
    qDBusRegisterMetaType<WireSecret>();
    qDBusRegisterMetaType<QMap<QString, QString>>();
}

void ObsSecretStoreTest::noKeyringIsAnErrorNotAnEmptyPassword() {
    PrivateBus bus;
    QVERIFY(bus.start());
    // Nothing owns org.freedesktop.secrets on this bus.
    SecretServiceObsStore store(bus.connection);
    QString error;
    // AGENT-GUARD: this must NOT look like "no secret yet", or a caller
    // would generate a new password over one it simply could not read.
    QVERIFY(!store.password(&error).has_value());
    QVERIFY(!error.isEmpty());
    QString writeError;
    QVERIFY(!store.setPassword(QStringLiteral("pw"), &writeError));
    QVERIFY(!writeError.isEmpty());
}

void ObsSecretStoreTest::anAbsentSecretIsTheOrdinaryFirstRunState() {
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeSecretService service;
    QVERIFY(service.publish(bus.connection));

    SecretServiceObsStore store(bus.connection);
    QString error;
    QVERIFY(!store.password(&error).has_value());
    // No diagnostic: there is simply nothing stored yet.
    QVERIFY2(error.isEmpty(), qPrintable(error));
    // And the search was scoped to this one secret.
    QCOMPARE(service.lastSearch, SecretServiceObsStore::attributes());
}

void ObsSecretStoreTest::storingUsesTheScopedAttributesAndReplaces() {
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeSecretService service;
    FakeSecretCollection collection;
    QVERIFY(service.publish(bus.connection));
    QVERIFY(collection.publish(bus.connection));

    SecretServiceObsStore store(bus.connection);
    QString error;
    QVERIFY2(store.setPassword(QStringLiteral("a-real-secret"), &error),
             qPrintable(error));
    QCOMPARE(collection.creates, 1);
    QCOMPARE(collection.lastValue, QByteArray("a-real-secret"));
    // Replace, so a repair updates the one password instead of leaving a
    // drawer full of stale items.
    QVERIFY(collection.lastReplace);
    // The Attributes property must arrive as a{ss}; a QVariantMap would
    // marshal as a{sv} and the keyring would store nothing searchable.
    const QDBusArgument attributeArgument =
        collection.lastProperties
            .value(QStringLiteral("org.freedesktop.Secret.Item.Attributes"))
            .value<QDBusArgument>();
    QCOMPARE(attributeArgument.currentType(), QDBusArgument::MapType);
    QMap<QString, QString> attributes;
    attributeArgument >> attributes;
    QCOMPARE(attributes, SecretServiceObsStore::attributes());
    QVERIFY(collection.lastProperties
                .value(QStringLiteral("org.freedesktop.Secret.Item.Label"))
                .toString()
                .contains(QStringLiteral("obs-websocket")));
    // Plain transfer on the user's own session bus.
    QCOMPARE(service.lastAlgorithm, QStringLiteral("plain"));
}

void ObsSecretStoreTest::aStoredSecretComesBackVerbatim() {
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeSecretService service;
    FakeSecretItem item;
    service.hasItem = true;
    item.value = QByteArray("a-real-secret");
    QVERIFY(service.publish(bus.connection));
    QVERIFY(item.publish(bus.connection));

    SecretServiceObsStore store(bus.connection);
    QString error;
    const auto password = store.password(&error);
    QVERIFY2(password.has_value(), qPrintable(error));
    QCOMPARE(*password, QStringLiteral("a-real-secret"));
}

void ObsSecretStoreTest::aRefusedStoreIsReported() {
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeSecretService service;
    FakeSecretCollection collection;
    collection.refuse = true;
    QVERIFY(service.publish(bus.connection));
    QVERIFY(collection.publish(bus.connection));

    SecretServiceObsStore store(bus.connection);
    QString error;
    QVERIFY(!store.setPassword(QStringLiteral("pw"), &error));
    QVERIFY(!error.isEmpty());
}

void ObsSecretStoreTest::anEmptyPasswordIsNeverStored() {
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeSecretService service;
    FakeSecretCollection collection;
    QVERIFY(service.publish(bus.connection));
    QVERIFY(collection.publish(bus.connection));

    SecretServiceObsStore store(bus.connection);
    QString error;
    QVERIFY(!store.setPassword(QString(), &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(collection.creates, 0);
}

QTEST_MAIN(ObsSecretStoreTest)
#include "tst_obs_secret_store.moc"
