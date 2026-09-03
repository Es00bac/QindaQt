// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboardappletcomposition.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/services/clipboard_model/clipboard_descriptor.h"
#include "qindaqt/services/clipboard_protocol/clipboard_dbus.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/shell/clipboard_applet/clipboard_applet_controller.h"

#include <QDBusConnection>
#include <QProcess>
#include <QStandardPaths>
#include <QtTest>
#include <QUuid>

#include <limits>

using namespace QindaQt;

namespace {

class PrivateSessionBus final {
public:
    ~PrivateSessionBus() { stop(); }

    bool start(QString *error)
    {
        const QString address = QStringLiteral("unix:abstract=qindaqt-clipboard-%1")
                                    .arg(QUuid::createUuid().toString(QUuid::Id128));
        m_process.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                        {QStringLiteral("--session"), QStringLiteral("--nofork"),
                         QStringLiteral("--nopidfile"),
                         QStringLiteral("--address=%1").arg(address),
                         QStringLiteral("--print-address=1")});
        if (!m_process.waitForStarted(5'000)
            || !m_process.waitForReadyRead(5'000)) {
            *error = m_process.errorString();
            return false;
        }
        m_address = QString::fromUtf8(m_process.readLine()).trimmed();
        return !m_address.isEmpty();
    }

    void stop()
    {
        if (m_process.state() == QProcess::NotRunning) {
            return;
        }
        m_process.terminate();
        if (!m_process.waitForFinished(1'000)) {
            m_process.kill();
            (void)m_process.waitForFinished(1'000);
        }
    }

    [[nodiscard]] QString address() const { return m_address; }

private:
    QProcess m_process;
    QString m_address;
};

class FakeSettingsTransport final
    : public Services::SettingsClient::SettingsTransport
{
    Q_OBJECT
public:
    bool start(QString *) override { return true; }
    void stop() override {}
    void commit(quint64, const QString &, const QString &, quint64,
                const QVariantList &) override {}
    void requestActivation() override {}

    void requestSnapshot(quint64 token, const QString &owner,
                         const QStringList &keys) override
    {
        QCOMPARE(keys, QStringList{QStringLiteral("services.clipboardHistory")});
        const QVariantMap wire = m_wire;
        QMetaObject::invokeMethod(this, [this, token, owner, wire] {
            Q_EMIT snapshotReceived(token, owner, wire);
        }, Qt::QueuedConnection);
    }

    void setReply(const QVariant &value, const QString &sourceLayer,
                  quint64 revision, bool includeKey = true)
    {
        using Services::SettingsProtocol::SettingsWireStatus;
        using Services::SettingsProtocol::WireContract;
        QVariantMap values;
        QVariantMap sources;
        if (includeKey) {
            values.insert(QStringLiteral("services.clipboardHistory"), value);
            sources.insert(QStringLiteral("services.clipboardHistory"), sourceLayer);
        }
        m_wire = {
            {QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("settings-a")},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
    }

    void announceOwner(const QString &owner = QStringLiteral(":settings.1"))
    {
        Q_EMIT ownerChanged(owner);
    }

private:
    QVariantMap m_wire;
};

class FakeClipboard1 final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Clipboard1")
public:
    explicit FakeClipboard1(Services::Clipboard::Snapshot snapshot)
        : m_snapshot(std::move(snapshot))
    {
    }

    void publish(Services::Clipboard::Snapshot snapshot)
    {
        m_snapshot = std::move(snapshot);
        Q_EMIT Changed(m_snapshot.epoch, m_snapshot.generation,
                       m_snapshot.revision);
    }

    [[nodiscard]] int copyCalls() const noexcept { return m_copyCalls; }

public Q_SLOTS:
    Q_SCRIPTABLE Services::Clipboard::Snapshot GetSnapshot() const
    {
        return m_snapshot;
    }
    Q_SCRIPTABLE Services::Clipboard::OperationResult Copy(
        quint64 requestId, quint64 epoch, quint32 generation, quint64 revision,
        const Services::ClipboardModel::EntryId &entry)
    {
        ++m_copyCalls;
        return result(Services::Clipboard::OperationKind::Copy, requestId,
                      epoch, generation, revision, entry);
    }
    Q_SCRIPTABLE Services::Clipboard::OperationResult Delete(
        quint64 requestId, quint64 epoch, quint32 generation, quint64 revision,
        const Services::ClipboardModel::EntryId &entry)
    {
        return result(Services::Clipboard::OperationKind::Delete, requestId,
                      epoch, generation, revision, entry);
    }
    Q_SCRIPTABLE Services::Clipboard::OperationResult Clear(
        quint64 requestId, quint64 epoch, quint32 generation, quint64 revision,
        bool)
    {
        return result(Services::Clipboard::OperationKind::Clear, requestId,
                      epoch, generation, revision, {});
    }
    Q_SCRIPTABLE Services::Clipboard::OperationResult Select(
        quint64 requestId, quint64 epoch, quint32 generation, quint64 revision,
        const Services::ClipboardModel::EntryId &entry)
    {
        return result(Services::Clipboard::OperationKind::Select, requestId,
                      epoch, generation, revision, entry);
    }

Q_SIGNALS:
    Q_SCRIPTABLE void Changed(quint64 epoch, quint32 generation, quint64 revision);

private:
    static Services::Clipboard::OperationResult result(
        Services::Clipboard::OperationKind kind, quint64 requestId,
        quint64 epoch, quint32 generation, quint64 revision,
        Services::ClipboardModel::EntryId)
    {
        return {.kind = kind,
                .status = Services::Clipboard::OperationStatus::Succeeded,
                .requestId = requestId,
                .initiatingEpoch = epoch,
                .initiatingGeneration = generation,
                .initiatingRevision = revision,
                .observedEpoch = epoch,
                .observedGeneration = generation,
                .observedRevision = revision,
                .reasonCode = QStringLiteral("ok")};
    }

    Services::Clipboard::Snapshot m_snapshot;
    int m_copyCalls = 0;
};

Services::ClipboardModel::ClipboardEntryDescriptor descriptor(quint32 generation)
{
    Services::ClipboardModel::ClipboardEntryDescriptor entry;
    entry.id = {generation, 1};
    entry.admittedTick = 1;
    entry.lastUsedTick = 1;
    entry.sourceLabel = QStringLiteral("Private bus fixture");
    entry.preview = QStringLiteral("alpha");
    entry.fingerprint = QByteArray(32, 'a');
    entry.formats = {{QStringLiteral("text/plain"), 5}};
    return entry;
}

Services::Clipboard::Snapshot snapshot(
    quint32 generation, bool historyEnabled, bool privacyAllowed, quint64 revision,
    QList<Services::ClipboardModel::ClipboardEntryDescriptor> entries)
{
    const auto encoded = Services::ClipboardModel::encodeDescriptorList(entries);
    Q_ASSERT(encoded.accepted());
    return {.epoch = 7,
            .generation = generation,
            .revision = revision,
            .historyEnabled = historyEnabled,
            .privacyAllowed = privacyAllowed,
            .descriptorList = encoded.bytes};
}

QString connectionName(QStringView role)
{
    return QStringLiteral("qindaqt-clipboard-composition-%1-%2")
        .arg(role, QUuid::createUuid().toString(QUuid::Id128));
}

} // namespace

class ClipboardAppletCompositionPrivateBusTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void composesConsentOwnerOperationsAndCeilingPurge();
    void withholdsDeniedConsentUntilFencedEmptySnapshot_data();
    void withholdsDeniedConsentUntilFencedEmptySnapshot();
};

void ClipboardAppletCompositionPrivateBusTests::
    composesConsentOwnerOperationsAndCeilingPurge()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    const QString serverName = connectionName(u"server");
    const QString clientName = connectionName(u"client");
    auto server = QDBusConnection::connectToBus(bus.address(), serverName);
    auto clientBus = QDBusConnection::connectToBus(bus.address(), clientName);
    QVERIFY(server.isConnected());
    QVERIFY(clientBus.isConnected());

    Services::Clipboard::registerDBusTypes();
    FakeClipboard1 service(snapshot(
        std::numeric_limits<quint32>::max(), true, true, 7,
        {descriptor(std::numeric_limits<quint32>::max())}));
    QVERIFY(server.registerObject(QString::fromLatin1(Services::Clipboard::kObjectPath),
                                  &service,
                                  QDBusConnection::ExportScriptableSlots
                                      | QDBusConnection::ExportScriptableSignals));
    QVERIFY(server.registerService(
        QString::fromLatin1(Services::Clipboard::kServiceName)));

    FakeSettingsTransport settingsTransport;
    settingsTransport.setReply(true, QStringLiteral("user-overrides"), 1);
    Services::SettingsClient::SettingsClient settings(
        settingsTransport, {QStringLiteral("services.clipboardHistory")},
        {.requestTimeoutMilliseconds = 1'000,
         .debounceMilliseconds = 0,
         .retryMilliseconds = {10}});
    QVERIFY(settings.start(&error));
    settingsTransport.announceOwner();
    QTRY_COMPARE(settings.state(), Services::SettingsClient::ClientState::Ready);

    Applets::ManifestCatalog catalog;
    QVERIFY2(catalog.loadDirectory(
                 QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets"), &error),
             qPrintable(error));
    const auto policy = AppletHost::CapabilityPolicyLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/applet-policy/default.json"));
    QVERIFY2(policy.ok, qPrintable(policy.error));

    Shell::ClipboardAppletComposition composition(
        catalog, policy.policy, settings, clientBus);
    auto *controller = composition.access();
    QVERIFY(controller != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(controller->phaseText(), QStringLiteral("ready"), 5'000);
    QCOMPARE(controller->entryCount(), 1);
    QVERIFY(controller->selectEntry(std::numeric_limits<quint32>::max(), 1));
    QTRY_COMPARE(service.copyCalls(), 1);
    QTRY_COMPARE(controller->pendingOperationCount(), 0);

    service.publish(snapshot(std::numeric_limits<quint32>::max(), true, false,
                             7, {}));
    QTRY_COMPARE(controller->phaseText(), QStringLiteral("locked"));
    QCOMPARE(controller->phaseReasonText(),
             QStringLiteral("Clipboard history is withheld by privacy policy."));
    QCOMPARE(controller->entryCount(), 0);

    service.publish(snapshot(std::numeric_limits<quint32>::max(), true, true,
                             7, {}));
    QTRY_COMPARE(controller->phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(controller->phaseReasonText(),
             QStringLiteral("Clipboard service unavailable: "
                            "lineage-exhausted-restart-required"));

    QVERIFY(server.unregisterService(
        QString::fromLatin1(Services::Clipboard::kServiceName)));
    QTRY_COMPARE(controller->entryCount(), 0);
    QVERIFY(controller->phaseText() == QLatin1String("unavailable"));

    server.unregisterObject(QString::fromLatin1(Services::Clipboard::kObjectPath));
    QDBusConnection::disconnectFromBus(clientName);
    QDBusConnection::disconnectFromBus(serverName);
    clientBus = QDBusConnection(QStringLiteral("released-client"));
    server = QDBusConnection(QStringLiteral("released-server"));
}

void ClipboardAppletCompositionPrivateBusTests::
    withholdsDeniedConsentUntilFencedEmptySnapshot_data()
{
    QTest::addColumn<QVariant>("consentValue");
    QTest::addColumn<QString>("sourceLayer");
    QTest::addColumn<bool>("includeKey");
    QTest::addColumn<bool>("announceInitialOwner");
    QTest::addColumn<int>("expectedSettingsState");
    QTest::addColumn<QString>("expectedInitialReason");

    using SettingsState = Services::SettingsClient::ClientState;
    QTest::newRow("missing")
        << QVariant{} << QString{} << false << true
        << int(SettingsState::Degraded)
        << QStringLiteral("clipboard-consent-unavailable");
    QTest::newRow("malformed")
        << QVariant{QStringLiteral("true")} << QStringLiteral("user-overrides")
        << true << true << int(SettingsState::Ready)
        << QStringLiteral("clipboard-consent-denied");
    QTest::newRow("inherited-default")
        << QVariant{true} << QStringLiteral("system-defaults") << true << true
        << int(SettingsState::Ready)
        << QStringLiteral("clipboard-consent-denied");
    QTest::newRow("ownerless")
        << QVariant{true} << QStringLiteral("user-overrides") << true << false
        << int(SettingsState::Authenticating)
        << QStringLiteral("clipboard-consent-unavailable");
}

void ClipboardAppletCompositionPrivateBusTests::
    withholdsDeniedConsentUntilFencedEmptySnapshot()
{
    QFETCH(QVariant, consentValue);
    QFETCH(QString, sourceLayer);
    QFETCH(bool, includeKey);
    QFETCH(bool, announceInitialOwner);
    QFETCH(int, expectedSettingsState);
    QFETCH(QString, expectedInitialReason);

    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    const QString serverName = connectionName(u"denial-server");
    const QString clientName = connectionName(u"denial-client");
    auto server = QDBusConnection::connectToBus(bus.address(), serverName);
    auto clientBus = QDBusConnection::connectToBus(bus.address(), clientName);
    QVERIFY(server.isConnected());
    QVERIFY(clientBus.isConnected());

    constexpr quint32 initialGeneration = 41;
    constexpr quint32 purgedGeneration = initialGeneration + 1;
    Services::Clipboard::registerDBusTypes();
    FakeClipboard1 service(snapshot(
        initialGeneration, true, true, 7, {descriptor(initialGeneration)}));
    QVERIFY(server.registerObject(QString::fromLatin1(Services::Clipboard::kObjectPath),
                                  &service,
                                  QDBusConnection::ExportScriptableSlots
                                      | QDBusConnection::ExportScriptableSignals));
    QVERIFY(server.registerService(
        QString::fromLatin1(Services::Clipboard::kServiceName)));

    FakeSettingsTransport settingsTransport;
    settingsTransport.setReply(consentValue, sourceLayer, 1, includeKey);
    Services::SettingsClient::SettingsClient settings(
        settingsTransport, {QStringLiteral("services.clipboardHistory")},
        {.requestTimeoutMilliseconds = 1'000,
         .debounceMilliseconds = 0,
         .retryMilliseconds = {30'000}});
    QVERIFY(settings.start(&error));
    if (announceInitialOwner) {
        settingsTransport.announceOwner();
    }
    QTRY_COMPARE(int(settings.state()), expectedSettingsState);

    Applets::ManifestCatalog catalog;
    QVERIFY2(catalog.loadDirectory(
                 QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets"), &error),
             qPrintable(error));
    const auto policy = AppletHost::CapabilityPolicyLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/applet-policy/default.json"));
    QVERIFY2(policy.ok, qPrintable(policy.error));

    Shell::ClipboardAppletComposition composition(
        catalog, policy.policy, settings, clientBus);
    auto *controller = composition.access();
    QVERIFY(controller != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(
        controller->phaseReasonText(),
        QStringLiteral("Clipboard service unavailable: %1")
            .arg(expectedInitialReason),
        5'000);
    QCOMPARE(controller->phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(controller->entryCount(), 0);

    // AGENT-NOTE: Normalize every denial row to an explicit false override
    // while Clipboard1 still exposes content. This makes each registered row
    // bite if the composition's immediate withholding branch is removed.
    settingsTransport.setReply(false, QStringLiteral("user-overrides"), 2);
    if (announceInitialOwner) {
        settings.refresh();
    } else {
        settingsTransport.announceOwner();
    }
    QTRY_COMPARE(settings.state(), Services::SettingsClient::ClientState::Ready);
    QTRY_COMPARE(controller->phaseReasonText(),
                 QStringLiteral("Clipboard service unavailable: "
                                "clipboard-consent-denied"));
    QCOMPARE(controller->phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(controller->entryCount(), 0);

    service.publish(snapshot(purgedGeneration, false, true, 7, {}));
    QTRY_COMPARE(controller->phaseText(), QStringLiteral("disabled"));
    QCOMPARE(controller->phaseReasonText(),
             QStringLiteral("Clipboard history is disabled."));
    QVERIFY(!controller->isHistoryEnabled());
    QCOMPARE(controller->entryCount(), 0);

    QVERIFY(server.unregisterService(
        QString::fromLatin1(Services::Clipboard::kServiceName)));
    server.unregisterObject(QString::fromLatin1(Services::Clipboard::kObjectPath));
    QDBusConnection::disconnectFromBus(clientName);
    QDBusConnection::disconnectFromBus(serverName);
    clientBus = QDBusConnection(QStringLiteral("released-denial-client"));
    server = QDBusConnection(QStringLiteral("released-denial-server"));
}

QTEST_GUILESS_MAIN(ClipboardAppletCompositionPrivateBusTests)
#include "tst_clipboard_applet_composition_private_bus.moc"
