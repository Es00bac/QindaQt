// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor/shellwindowidentity.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace QindaQt::Compositor;

namespace {

const QString Epoch = QStringLiteral("8cd615b4-69c2-4c6d-97c6-bef6a39e533d");
const QString WindowId = QStringLiteral("12345678-1234-4234-8234-123456789abc");

ShellWindowIdentityCandidate candidate()
{
    return {{Epoch, 7}, ShellWindowIdentityFacts{
        WindowId, qint64(4321), quint32(0x12345),
        QStringLiteral(":1.42"), QStringLiteral("/com/example/Menu")}};
}

class FakeCredentials final : public ShellWindowCredentialSource
{
public:
    std::optional<qint64> processIdForUniqueName(
        const QString &name) const override
    {
        ++calls;
        return values.value(name, std::nullopt);
    }
    QHash<QString, std::optional<qint64>> values;
    mutable int calls = 0;
};

class FakeOwner final : public ShellPanelOwnerSource
{
public:
    std::optional<qint64> shellPanelProcessId() const override { return pid; }
    std::optional<qint64> pid = 100;
};

class FakeSource final : public ShellWindowIdentitySource
{
public:
    const QByteArray &snapshotJson() override
    {
        ++calls;
        return payload;
    }
    QByteArray payload;
    int calls = 0;
};

} // namespace

class ShellWindowIdentityTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publishesTypedFactsAndActionFence();
    void publishesTypedAbsences();
    void outageAdvancesIdentityLineage();
    void rejectsMalformedOrPartialFactsAtomically();
    void rejectsInvalidActionGenerationEpochs();
    void authenticatesBeforeConsultingIdentitySource();
    void codecRejectsHostileTypesAndChangedFields();
};

void ShellWindowIdentityTest::publishesTypedFactsAndActionFence()
{
    ShellWindowIdentityStore store(Epoch);
    QCOMPARE(store.publish(candidate()), ShellWindowIdentityPublishResult::Published);
    const auto decoded = decodeShellWindowIdentitySnapshot(store.snapshotJson());
    QVERIFY(decoded);
    QVERIFY(decoded->available());
    QCOMPARE(decoded->revision, quint64(1));
    QCOMPARE(decoded->actionGeneration, (ShellWindowGeneration{Epoch, 7}));
    QVERIFY(decoded->activeWindow);
    QCOMPARE(decoded->activeWindow->processId, std::optional<qint64>(4321));
    QCOMPARE(decoded->activeWindow->appMenuWindowId,
             std::optional<quint32>(0x12345));
    QCOMPARE(decoded->activeWindow->appMenuServiceName,
             std::optional<QString>(QStringLiteral(":1.42")));
    QCOMPARE(decoded->activeWindow->appMenuObjectPath,
             std::optional<QString>(QStringLiteral("/com/example/Menu")));
    QCOMPARE(store.publish(candidate()), ShellWindowIdentityPublishResult::Unchanged);
    QCOMPARE(store.revision(), quint64(1));
}

void ShellWindowIdentityTest::publishesTypedAbsences()
{
    ShellWindowIdentityStore store(Epoch);
    auto value = candidate();
    value.activeWindow->processId.reset();
    value.activeWindow->appMenuWindowId.reset();
    value.activeWindow->appMenuServiceName.reset();
    value.activeWindow->appMenuObjectPath.reset();
    QCOMPARE(store.publish(value), ShellWindowIdentityPublishResult::Published);
    const QJsonObject active = QJsonDocument::fromJson(store.snapshotJson())
                                   .object().value(QStringLiteral("activeWindow")).toObject();
    QVERIFY(active.value(QStringLiteral("processId")).isNull());
    QVERIFY(active.value(QStringLiteral("appMenuWindowId")).isNull());
    QVERIFY(active.value(QStringLiteral("appMenuServiceName")).isNull());
    QVERIFY(active.value(QStringLiteral("appMenuObjectPath")).isNull());

    value.actionGeneration.revision = 8;
    value.activeWindow.reset();
    QCOMPARE(store.publish(value), ShellWindowIdentityPublishResult::Published);
    const auto decoded = decodeShellWindowIdentitySnapshot(store.snapshotJson());
    QVERIFY(decoded && !decoded->activeWindow);
}

void ShellWindowIdentityTest::outageAdvancesIdentityLineage()
{
    ShellWindowIdentityStore store(Epoch);
    QCOMPARE(store.publish(candidate()), ShellWindowIdentityPublishResult::Published);
    QVERIFY(store.markUnavailable(QStringLiteral("sampling-failed"),
                                  QStringLiteral("identity source failed")));
    const auto unavailable = decodeShellWindowIdentitySnapshot(store.snapshotJson());
    QVERIFY(unavailable);
    QCOMPARE(unavailable->status, ShellWindowIdentityStatus::Unavailable);
    QCOMPARE(unavailable->revision, quint64(2));

    QCOMPARE(store.publish(candidate()), ShellWindowIdentityPublishResult::Published);
    QCOMPARE(store.revision(), quint64(3));
}

void ShellWindowIdentityTest::rejectsMalformedOrPartialFactsAtomically()
{
    ShellWindowIdentityStore store(Epoch);
    QCOMPARE(store.publish(candidate()), ShellWindowIdentityPublishResult::Published);
    const QByteArray retained = store.snapshotJson();
    auto malformed = candidate();
    malformed.activeWindow->appMenuObjectPath.reset();
    QCOMPARE(store.publish(malformed), ShellWindowIdentityPublishResult::Rejected);
    QCOMPARE(store.snapshotJson(), retained);
    malformed = candidate();
    malformed.activeWindow->processId = 1;
    QCOMPARE(store.publish(malformed), ShellWindowIdentityPublishResult::Rejected);
    QCOMPARE(store.snapshotJson(), retained);
}

void ShellWindowIdentityTest::rejectsInvalidActionGenerationEpochs()
{
    ShellWindowIdentityStore store(Epoch);
    auto malformed = candidate();
    malformed.actionGeneration.epoch = QStringLiteral(" padded-epoch ");
    QCOMPARE(store.publish(malformed), ShellWindowIdentityPublishResult::Rejected);
    QCOMPARE(store.revision(), quint64(0));

    ShellWindowIdentitySnapshot snapshot{
        ShellWindowIdentityStatus::Ok, QStringLiteral(" padded-epoch "), 1,
        {QStringLiteral(" padded-epoch "), 7}, std::nullopt, {}, {}};
    QString error;
    QVERIFY(!decodeShellWindowIdentitySnapshot(
        encodeShellWindowIdentitySnapshot(snapshot), &error));
    QVERIFY(!error.isEmpty());

    snapshot.status = ShellWindowIdentityStatus::Unavailable;
    snapshot.actionGeneration = {};
    snapshot.failureCode = QStringLiteral("identity-unavailable");
    snapshot.message = QStringLiteral("test failure");
    QVERIFY(!decodeShellWindowIdentitySnapshot(
        encodeShellWindowIdentitySnapshot(snapshot), &error));
}

void ShellWindowIdentityTest::authenticatesBeforeConsultingIdentitySource()
{
    ShellWindowIdentityStore store(Epoch);
    QCOMPARE(store.publish(candidate()), ShellWindowIdentityPublishResult::Published);
    FakeCredentials credentials;
    credentials.values.insert(QStringLiteral(":1.shell"), 100);
    credentials.values.insert(QStringLiteral(":1.hostile"), 101);
    FakeOwner owner;
    FakeSource source;
    source.payload = store.snapshotJson();
    ShellWindowIdentityController controller(credentials, owner, source);

    const QByteArray denied = controller.snapshot(QStringLiteral(":1.hostile"));
    QCOMPARE(source.calls, 0);
    QVERIFY(denied.size() < 512);
    const auto deniedValue = decodeShellWindowIdentitySnapshot(denied);
    QVERIFY(deniedValue);
    QCOMPARE(deniedValue->status, ShellWindowIdentityStatus::Unauthorized);
    QVERIFY(!denied.contains("windowId"));

    QCOMPARE(controller.snapshot(QStringLiteral(":1.shell")), source.payload);
    QCOMPARE(source.calls, 1);
    owner.pid.reset();
    const auto revoked = decodeShellWindowIdentitySnapshot(
        controller.snapshot(QStringLiteral(":1.shell")));
    QVERIFY(revoked);
    QCOMPARE(revoked->status, ShellWindowIdentityStatus::Unauthorized);
    QCOMPARE(source.calls, 1);
}

void ShellWindowIdentityTest::codecRejectsHostileTypesAndChangedFields()
{
    ShellWindowIdentityStore store(Epoch);
    QCOMPARE(store.publish(candidate()), ShellWindowIdentityPublishResult::Published);
    QJsonObject root = QJsonDocument::fromJson(store.snapshotJson()).object();
    QJsonObject active = root.value(QStringLiteral("activeWindow")).toObject();
    active.insert(QStringLiteral("processId"), 4321);
    root.insert(QStringLiteral("activeWindow"), active);
    QVERIFY(!decodeShellWindowIdentitySnapshot(
        QJsonDocument(root).toJson(QJsonDocument::Compact)));

    active = QJsonDocument::fromJson(store.snapshotJson())
                 .object().value(QStringLiteral("activeWindow")).toObject();
    active.insert(QStringLiteral("appMenuWindowId"), -1);
    root = QJsonDocument::fromJson(store.snapshotJson()).object();
    root.insert(QStringLiteral("activeWindow"), active);
    QVERIFY(!decodeShellWindowIdentitySnapshot(
        QJsonDocument(root).toJson(QJsonDocument::Compact)));

    QVERIFY(!decodeShellWindowIdentitySnapshot(QByteArray(65 * 1024, 'x')));
}

QTEST_GUILESS_MAIN(ShellWindowIdentityTest)
#include "tst_shellwindowidentity.moc"
