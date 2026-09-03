// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor/shellwindowactions.h"

#include <QTest>

using namespace QindaQt::Compositor;

namespace {

const QString WindowId = QStringLiteral("12345678-1234-4234-8234-123456789abc");
const ShellWindowGeneration Generation{QStringLiteral("epoch-1"), 9};

class FakeCredentials final : public ShellWindowCredentialSource
{
public:
    std::optional<qint64> processIdForUniqueName(
        const QString &uniqueName) const override
    {
        return processIds.value(uniqueName, std::nullopt);
    }
    QHash<QString, std::optional<qint64>> processIds;
};

class FakeOwner final : public ShellPanelOwnerSource
{
public:
    std::optional<qint64> shellPanelProcessId() const override { return processId; }
    std::optional<qint64> processId = 100;
};

class FakeRegistry final : public ShellWindowRegistry
{
public:
    std::optional<ShellWindowGeneration> currentGeneration() const override
    {
        return generation;
    }
    std::optional<ShellWindowTarget> target(const QString &windowId) const override
    {
        ++lookups;
        return targets.value(windowId, std::nullopt);
    }
    std::optional<ShellWindowGeneration> generation = Generation;
    QHash<QString, std::optional<ShellWindowTarget>> targets{
        {WindowId, ShellWindowTarget{WindowId, {}}}};
    mutable int lookups = 0;
};

class FakeExecutor final : public ShellWindowActionExecutor
{
public:
    bool execute(ShellWindowAction action, const ShellWindowTarget &target,
                 QString *error) override
    {
        ++calls;
        lastAction = action;
        lastTarget = target;
        if (!succeeds && error) *error = QStringLiteral("policy failed");
        return succeeds;
    }
    int calls = 0;
    bool succeeds = true;
    ShellWindowAction lastAction = ShellWindowAction::Activate;
    ShellWindowTarget lastTarget;
};

struct Fixture final
{
    Fixture(ShellWindowActionLimits limits = {}, ShellActionClock clock = {})
        : controller(credentials, owner, registry, executor, limits, std::move(clock))
    {
        credentials.processIds.insert(QStringLiteral(":1.good"), 100);
        credentials.processIds.insert(QStringLiteral(":1.bad"), 101);
    }

    ShellWindowActionRequest request(
        ShellWindowAction action = ShellWindowAction::Activate) const
    {
        return {QStringLiteral(":1.good"), action, WindowId, Generation};
    }

    FakeCredentials credentials;
    FakeOwner owner;
    FakeRegistry registry;
    FakeExecutor executor;
    ShellWindowActionController controller;
};

} // namespace

class ShellWindowActionsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void admitsOnlyBoundPid_data();
    void admitsOnlyBoundPid();
    void staleGenerationPrecedesWindowLookup();
    void unknownWindowDoesNotExecute();
    void unboundOwnerDisablesBeforeCredentials();
    void preservesHybridRouteForExecutor();
    void boundsRateAndResetsAfterInterval();
    void executorFailureIsControlDisabled();
    void codecRoundTripAndRejectsMismatch();
};

void ShellWindowActionsTest::admitsOnlyBoundPid_data()
{
    QTest::addColumn<int>("actionValue");
    QTest::newRow("activate") << int(ShellWindowAction::Activate);
    QTest::newRow("minimize") << int(ShellWindowAction::Minimize);
    QTest::newRow("unminimize") << int(ShellWindowAction::Unminimize);
    QTest::newRow("close") << int(ShellWindowAction::Close);
    QTest::newRow("raise") << int(ShellWindowAction::Raise);
}

void ShellWindowActionsTest::admitsOnlyBoundPid()
{
    QFETCH(int, actionValue);
    const auto action = static_cast<ShellWindowAction>(actionValue);
    Fixture fixture;
    auto bad = fixture.request(action);
    bad.callerUniqueName = QStringLiteral(":1.bad");
    QCOMPARE(fixture.controller.submit(bad).status,
             ShellWindowActionStatus::Unauthorized);
    QCOMPARE(fixture.executor.calls, 0);
    const auto admitted = fixture.controller.submit(fixture.request(action));
    QCOMPARE(admitted.status, ShellWindowActionStatus::Admitted);
    QCOMPARE(fixture.executor.calls, 1);
    QCOMPARE(fixture.executor.lastAction, action);
}

void ShellWindowActionsTest::staleGenerationPrecedesWindowLookup()
{
    Fixture fixture;
    auto request = fixture.request();
    request.generation.revision = 8;
    QCOMPARE(fixture.controller.submit(request).status,
             ShellWindowActionStatus::Stale);
    QCOMPARE(fixture.registry.lookups, 0);
    QCOMPARE(fixture.executor.calls, 0);
}

void ShellWindowActionsTest::unknownWindowDoesNotExecute()
{
    Fixture fixture;
    auto request = fixture.request();
    request.windowId = QStringLiteral("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");
    QCOMPARE(fixture.controller.submit(request).status,
             ShellWindowActionStatus::UnknownWindow);
    QCOMPARE(fixture.executor.calls, 0);
}

void ShellWindowActionsTest::unboundOwnerDisablesBeforeCredentials()
{
    Fixture fixture;
    fixture.owner.processId.reset();
    auto request = fixture.request();
    request.callerUniqueName = QStringLiteral(":1.bad");
    const auto result = fixture.controller.submit(request);
    QCOMPARE(result.status, ShellWindowActionStatus::ControlDisabled);
    QCOMPARE(result.failureCode, QStringLiteral("shell-owner-unbound"));
}

void ShellWindowActionsTest::preservesHybridRouteForExecutor()
{
    Fixture fixture;
    fixture.registry.targets[WindowId] =
        ShellWindowTarget{WindowId, QStringLiteral("container-1")};
    QCOMPARE(fixture.controller.submit(fixture.request()).status,
             ShellWindowActionStatus::Admitted);
    QCOMPARE(fixture.executor.lastTarget.hybridContainerId,
             QStringLiteral("container-1"));
}

void ShellWindowActionsTest::boundsRateAndResetsAfterInterval()
{
    qint64 now = 100;
    Fixture fixture({2, 50}, [&now] { return now; });
    QCOMPARE(fixture.controller.submit(fixture.request()).status,
             ShellWindowActionStatus::Admitted);
    QCOMPARE(fixture.controller.submit(fixture.request()).status,
             ShellWindowActionStatus::Admitted);
    const auto limited = fixture.controller.submit(fixture.request());
    QCOMPARE(limited.status, ShellWindowActionStatus::ControlDisabled);
    QCOMPARE(limited.failureCode, QStringLiteral("rate-limited"));
    QCOMPARE(fixture.executor.calls, 2);
    now = 150;
    QCOMPARE(fixture.controller.submit(fixture.request()).status,
             ShellWindowActionStatus::Admitted);
}

void ShellWindowActionsTest::executorFailureIsControlDisabled()
{
    Fixture fixture;
    fixture.executor.succeeds = false;
    const auto result = fixture.controller.submit(fixture.request());
    QCOMPARE(result.status, ShellWindowActionStatus::ControlDisabled);
    QCOMPARE(result.failureCode, QStringLiteral("action-rejected"));
    QCOMPARE(result.message, QStringLiteral("policy failed"));
}

void ShellWindowActionsTest::codecRoundTripAndRejectsMismatch()
{
    const ShellWindowActionResult source{
        ShellWindowActionStatus::Stale, ShellWindowAction::Raise,
        WindowId, Generation, QStringLiteral("stale-generation"),
        QStringLiteral("stale")};
    QString error;
    const auto decoded = decodeShellWindowActionResult(
        encodeShellWindowActionResult(source), &error);
    QVERIFY2(decoded.has_value(), qPrintable(error));
    QCOMPARE(decoded->status, source.status);
    QCOMPARE(decoded->generation, source.generation);
    QVERIFY(!decodeShellWindowActionResult(QByteArray("{}"), &error));
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(ShellWindowActionsTest)
#include "tst_shellwindowactions.moc"
