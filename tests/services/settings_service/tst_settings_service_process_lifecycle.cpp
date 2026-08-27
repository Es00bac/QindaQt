// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_decode.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QtTest>

#include <csignal>
#include <optional>

using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {

struct ActivationObservation final {
    QString owner;
    QString epoch;
    qint64 processId = 0;
};

class PrivateBusDaemon final {
public:
    ~PrivateBusDaemon() { stop(); }

    bool start(const QProcessEnvironment &environment, QString *error)
    {
        m_process.setProcessEnvironment(environment);
        m_process.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                        {QStringLiteral("--session"), QStringLiteral("--nofork"),
                         QStringLiteral("--print-address=1")});
        if (!m_process.waitForStarted(5'000) || !m_process.waitForReadyRead(5'000)) {
            *error = m_process.errorString() + QStringLiteral(": ")
                     + QString::fromUtf8(m_process.readAllStandardError());
            stop();
            return false;
        }
        m_address = QString::fromUtf8(m_process.readLine()).trimmed();
        if (m_address.isEmpty()) {
            *error = QStringLiteral("private daemon returned no address: ")
                     + QString::fromUtf8(m_process.readAllStandardError());
            stop();
            return false;
        }
        return true;
    }

    void stop() noexcept
    {
        if (m_process.state() == QProcess::NotRunning) {
            return;
        }
        m_process.terminate();
        if (!m_process.waitForFinished(5'000)) {
            m_process.kill();
            m_process.waitForFinished(5'000);
        }
    }

    [[nodiscard]] const QString &address() const noexcept { return m_address; }

private:
    QProcess m_process;
    QString m_address;
};

class ActivatedProcessCleanup final {
public:
    explicit ActivatedProcessCleanup(QString executable)
        : m_executable(QFileInfo(std::move(executable)).canonicalFilePath())
    {
    }

    ~ActivatedProcessCleanup()
    {
        for (const qint64 processId : m_processIds) {
            terminateIfExactProcess(processId);
        }
    }

    void track(qint64 processId) { m_processIds.append(processId); }

    [[nodiscard]] bool exists(qint64 processId) const
    {
        return QFileInfo::exists(QStringLiteral("/proc/%1").arg(processId));
    }

private:
    [[nodiscard]] bool isExactProcess(qint64 processId) const
    {
        return QFileInfo(QStringLiteral("/proc/%1/exe").arg(processId)).canonicalFilePath()
               == m_executable;
    }

    void terminateIfExactProcess(qint64 processId) const noexcept
    {
        if (!isExactProcess(processId)) {
            return;
        }
        ::kill(static_cast<pid_t>(processId), SIGTERM);
        for (int attempt = 0; attempt < 50 && isExactProcess(processId); ++attempt) {
            QTest::qSleep(20);
        }
        if (isExactProcess(processId)) {
            ::kill(static_cast<pid_t>(processId), SIGKILL);
        }
    }

    QString m_executable;
    QList<qint64> m_processIds;
};

std::optional<ActivationObservation> activateAndObserve(const QString &address,
                                                        const QString &connectionName,
                                                        QString *error)
{
    auto connection = QDBusConnection::connectToBus(address, connectionName);
    if (!connection.isConnected()) {
        *error = connection.lastError().message();
        return std::nullopt;
    }

    auto message = QDBusMessage::createMethodCall(
        QString::fromLatin1(WireContract::ServiceName),
        QString::fromLatin1(WireContract::ObjectPath),
        QString::fromLatin1(WireContract::InterfaceName),
        QString::fromLatin1(WireContract::GetSnapshotMethod));
    message << QStringList{QStringLiteral("services.doNotDisturb")};
    const QDBusReply<QVariantMap> snapshot(connection.call(message, QDBus::Block, 5'000));
    if (!snapshot.isValid()) {
        *error = snapshot.error().message();
        QDBusConnection::disconnectFromBus(connectionName);
        return std::nullopt;
    }
    const QVariantMap reply = snapshot.value();
    if (reply.value(QLatin1StringView(WireContract::FieldStatus)).toUInt()
            != quint32(SettingsWireStatus::Applied)
        || reply.value(QLatin1StringView(WireContract::FieldEpoch)).toString().isEmpty()
        || reply.value(QLatin1StringView(WireContract::FieldRevision)).toULongLong() != 0) {
        *error = QStringLiteral("activated service returned a malformed initial snapshot");
        QDBusConnection::disconnectFromBus(connectionName);
        return std::nullopt;
    }

    QVariantMap operation{
        {QLatin1StringView(WireContract::FieldKey), QStringLiteral("unknown.key")},
        {QLatin1StringView(WireContract::FieldKind),
         QString::fromLatin1(WireContract::OperationKindSet)},
        {QLatin1StringView(WireContract::FieldValue), true}};
    auto commitMessage = QDBusMessage::createMethodCall(
        QString::fromLatin1(WireContract::ServiceName),
        QString::fromLatin1(WireContract::ObjectPath),
        QString::fromLatin1(WireContract::InterfaceName),
        QString::fromLatin1(WireContract::CommitUserTransactionMethod));
    commitMessage << reply.value(QLatin1StringView(WireContract::FieldEpoch)).toString()
                  << quint64(0) << QVariantList{operation};
    const QDBusReply<QVariantMap> commit(
        connection.call(commitMessage, QDBus::Block, 5'000));
    if (!commit.isValid()) {
        *error = QStringLiteral("UnknownKey probe failed: ") + commit.error().message();
        QDBusConnection::disconnectFromBus(connectionName);
        return std::nullopt;
    }
    const QVariantMap outcome = commit.value();
    const auto values = QindaQt::Services::SettingsProtocol::decodeBoundedVariantMap(
        outcome.value(QLatin1StringView(WireContract::FieldValues)), 1);
    const auto sources = QindaQt::Services::SettingsProtocol::decodeBoundedVariantMap(
        outcome.value(QLatin1StringView(WireContract::FieldSourceLayers)), 1);
    const auto changed = QindaQt::Services::SettingsProtocol::decodeBoundedKeyList(
        outcome.value(QLatin1StringView(WireContract::FieldChangedKeys)), 1);
    if (outcome.size() != WireContract::CommitReplyFieldCount
        || outcome.value(QLatin1StringView(WireContract::FieldStatus)).toUInt()
               != quint32(SettingsWireStatus::UnknownKey)
        || outcome.value(QLatin1StringView(WireContract::FieldEpoch)).toString()
               != reply.value(QLatin1StringView(WireContract::FieldEpoch)).toString()
        || outcome.value(QLatin1StringView(WireContract::FieldRevisionBefore)).toULongLong()
               != 0
        || outcome.value(QLatin1StringView(WireContract::FieldRevisionAfter)).toULongLong()
               != 0
        || !values || !values->isEmpty() || !sources || !sources->isEmpty() || !changed
        || !changed->isEmpty()) {
        *error = QStringLiteral("activated service returned a malformed UnknownKey outcome");
        QDBusConnection::disconnectFromBus(connectionName);
        return std::nullopt;
    }
    const QString outcomeMessage =
        outcome.value(QLatin1StringView(WireContract::FieldMessage)).toString();
    if (!outcomeMessage.contains(QStringLiteral("unknown.key"))
        || outcomeMessage.toUtf8().size() > WireContract::MaximumMessageBytes) {
        *error = QStringLiteral("activated service returned an invalid UnknownKey diagnostic");
        QDBusConnection::disconnectFromBus(connectionName);
        return std::nullopt;
    }

    const QDBusReply<QString> owner = connection.interface()->serviceOwner(
        QString::fromLatin1(WireContract::ServiceName));
    const QDBusReply<uint> processId = connection.interface()->servicePid(
        QString::fromLatin1(WireContract::ServiceName));
    if (!owner.isValid() || owner.value().isEmpty() || !processId.isValid()
        || processId.value() == 0) {
        *error = owner.isValid() ? processId.error().message() : owner.error().message();
        QDBusConnection::disconnectFromBus(connectionName);
        return std::nullopt;
    }

    const ActivationObservation observation{
        .owner = owner.value(),
        .epoch = reply.value(QLatin1StringView(WireContract::FieldEpoch)).toString(),
        .processId = static_cast<qint64>(processId.value())};
    QDBusConnection::disconnectFromBus(connectionName);
    return observation;
}

bool writeActivationDescriptor(const QString &dataRoot,
                               const QString &serviceExecutable,
                               QString *error)
{
    const QString serviceDirectory =
        QDir(dataRoot).filePath(QStringLiteral("dbus-1/services"));
    if (!QDir().mkpath(serviceDirectory)) {
        *error = QStringLiteral("cannot create activation service directory");
        return false;
    }
    QFile descriptor(QDir(serviceDirectory).filePath(
        QStringLiteral("org.qindaqt.Settings1.service")));
    if (!descriptor.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *error = descriptor.errorString();
        return false;
    }
    const QByteArray contents = QByteArrayLiteral("[D-BUS Service]\nName=")
                                + WireContract::ServiceName + QByteArrayLiteral("\nExec=")
                                + serviceExecutable.toUtf8()
                                + QByteArrayLiteral("\n");
    if (descriptor.write(contents) != contents.size()) {
        *error = descriptor.errorString();
        return false;
    }
    return true;
}

} // namespace

class SettingsServiceProcessLifecycleTests final : public QObject {
    Q_OBJECT
private slots:
    void exitsOnDaemonLossAndReactivatesWithoutOrphans();
};

void SettingsServiceProcessLifecycleTests::exitsOnDaemonLossAndReactivatesWithoutOrphans()
{
    QTemporaryDir directory;
    QVERIFY2(directory.isValid(), "could not create isolated process-test directory");
    const QString stagedDataRoot =
        qEnvironmentVariable("QINDAQT_TEST_SETTINGS_SERVICE_DATA_ROOT");
    const QString serviceExecutable = qEnvironmentVariable(
        "QINDAQT_TEST_SETTINGS_SERVICE_EXECUTABLE",
        QStringLiteral(QINDAQT_SETTINGS_SERVICE_EXECUTABLE));
    const QString schemaDirectory = qEnvironmentVariable(
        "QINDAQT_TEST_SETTINGS_SCHEMA_DIR",
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings"));
    const QString dataRoot = stagedDataRoot.isEmpty()
                                 ? QDir(directory.path()).filePath(QStringLiteral("data"))
                                 : stagedDataRoot;
    const QString configRoot = QDir(directory.path()).filePath(QStringLiteral("config"));
    QString error;
    if (stagedDataRoot.isEmpty()) {
        QVERIFY2(writeActivationDescriptor(dataRoot, serviceExecutable, &error),
                 qPrintable(error));
    } else {
        QVERIFY2(QFileInfo::exists(QDir(dataRoot).filePath(
                     QStringLiteral("dbus-1/services/org.qindaqt.Settings1.service"))),
                 "staged activation descriptor is missing");
    }
    QVERIFY2(QFileInfo::exists(serviceExecutable), "settings service executable is missing");
    QVERIFY2(QFileInfo::exists(QDir(schemaDirectory).filePath(QStringLiteral("schema-v2.json"))),
             "settings schema directory is missing schema-v2.json");
    QVERIFY(QDir().mkpath(configRoot));

    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("XDG_DATA_DIRS"), dataRoot);
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), configRoot);
    environment.insert(QStringLiteral("QINDAQT_SETTINGS_SCHEMA_DIR"), schemaDirectory);
    environment.remove(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"));
    environment.remove(QStringLiteral("DBUS_STARTER_ADDRESS"));
    environment.remove(QStringLiteral("DBUS_STARTER_BUS_TYPE"));

    ActivatedProcessCleanup cleanup(serviceExecutable);
    PrivateBusDaemon firstDaemon;
    QVERIFY2(firstDaemon.start(environment, &error), qPrintable(error));
    const auto first = activateAndObserve(
        firstDaemon.address(), QStringLiteral("settings-process-first"), &error);
    QVERIFY2(first.has_value(), qPrintable(error));
    cleanup.track(first->processId);
    QVERIFY(cleanup.exists(first->processId));
    QVERIFY(!QFileInfo::exists(
        QDir(configRoot).filePath(QStringLiteral("qindaqt/settings-v2.json"))));

    firstDaemon.stop();
    QTRY_VERIFY_WITH_TIMEOUT(!cleanup.exists(first->processId), 5'000);

    PrivateBusDaemon secondDaemon;
    QVERIFY2(secondDaemon.start(environment, &error), qPrintable(error));
    const QString placeholderName = QStringLiteral("settings-process-placeholder");
    auto placeholder = QDBusConnection::connectToBus(secondDaemon.address(), placeholderName);
    QVERIFY(placeholder.isConnected());
    const auto second = activateAndObserve(
        secondDaemon.address(), QStringLiteral("settings-process-second"), &error);
    QVERIFY2(second.has_value(), qPrintable(error));
    cleanup.track(second->processId);
    QVERIFY(cleanup.exists(second->processId));
    QVERIFY(!QFileInfo::exists(
        QDir(configRoot).filePath(QStringLiteral("qindaqt/settings-v2.json"))));
    QVERIFY(second->processId != first->processId);
    QVERIFY(second->owner != first->owner);
    QVERIFY(second->epoch != first->epoch);

    secondDaemon.stop();
    QTRY_VERIFY_WITH_TIMEOUT(!cleanup.exists(second->processId), 5'000);
    QDBusConnection::disconnectFromBus(placeholderName);
}

QTEST_GUILESS_MAIN(SettingsServiceProcessLifecycleTests)
#include "tst_settings_service_process_lifecycle.moc"
