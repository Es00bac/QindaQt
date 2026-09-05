// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor/shelltaskfacts.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace QindaQt::Compositor;

namespace {

constexpr auto Epoch = "3d975df8-3ee6-4cc5-bf64-3d46cab972d0";

ShellTaskFactsCandidate candidate()
{
    return {
        {QString::fromLatin1(Epoch), 8},
        {{QStringLiteral("WL-0")}},
        {{QStringLiteral("workspace-1")}},
        {{QStringLiteral("container-1"), 4,
          ShellTaskContainerAuthority::HybridProcess}},
        {
            {.windowId = QStringLiteral("window-1"),
             .applicationId = QStringLiteral("org.qindaqt.Settings"),
             .applicationName = QStringLiteral("Settings"),
             .title = QStringLiteral("Settings"),
             .role = ShellTaskWindowRole::Standalone,
             .outputId = QStringLiteral("WL-0"),
             .workspaceIds = {QStringLiteral("workspace-1")}},
            {.windowId = QStringLiteral("window-2"),
             .applicationId = QStringLiteral("org.qindaqt.Editor"),
             .applicationName = QStringLiteral("Editor"),
             .title = QStringLiteral("Document"),
             .role = ShellTaskWindowRole::ContainerPrimary,
             .active = true,
             .maximized = true,
             .outputId = QStringLiteral("WL-0"),
             .workspaceIds = {QStringLiteral("workspace-1")},
             .containerId = QStringLiteral("container-1")},
            {.windowId = QStringLiteral("window-3"),
             .applicationId = QStringLiteral("org.qindaqt.Terminal"),
             .applicationName = QStringLiteral("Terminal"),
             .title = QStringLiteral("Terminal"),
             .role = ShellTaskWindowRole::ContainerMember,
             .minimized = true,
             .demandsAttention = true,
             .outputId = QStringLiteral("WL-0"),
             .workspaceIds = {QStringLiteral("workspace-1")},
             .containerId = QStringLiteral("container-1")},
        },
    };
}

class Credentials final : public ShellWindowCredentialSource {
public:
    std::optional<qint64> processIdForUniqueName(
        const QString &uniqueName) const override
    {
        return pids.value(uniqueName);
    }
    QHash<QString, qint64> pids;
};

class PanelOwner final : public ShellPanelOwnerSource {
public:
    std::optional<qint64> shellPanelProcessId() const override { return pid; }
    std::optional<qint64> pid = 42;
};

class Source final : public ShellTaskFactsSource {
public:
    const QByteArray &snapshotJson() override
    {
        ++calls;
        return payload;
    }
    QByteArray payload = QByteArrayLiteral("{\"status\":\"source\"}");
    int calls = 0;
};

} // namespace

class ShellTaskFactsTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void publicationIsAtomicAndGenerationFenced();
    void roleAndOwnerProvenanceRoundTripsAndRejectsHostileValues();
    void hostileBoundsAndReferencesRetainTheGeneration();
    void authenticationPrecedesSourceInspection();
};

void ShellTaskFactsTests::publicationIsAtomicAndGenerationFenced()
{
    ShellTaskFactsStore store(QString::fromLatin1(Epoch));
    QString error;
    QCOMPARE(store.publish(candidate(), &error),
             ShellTaskFactsPublishResult::Published);
    auto decoded = decodeShellTaskFactsSnapshot(store.snapshotJson(), &error);
    QVERIFY2(decoded.has_value(), qPrintable(error));
    QVERIFY(decoded->available());
    QCOMPARE(decoded->revision, quint64(1));
    QCOMPARE(decoded->facts.actionGeneration.revision, quint64(8));
    QCOMPARE(decoded->facts.windows.size(), 3);
    QCOMPARE(decoded->facts.containers.constFirst().revision, quint64(4));

    QCOMPARE(store.publish(candidate(), &error),
             ShellTaskFactsPublishResult::Unchanged);
    QCOMPARE(store.revision(), quint64(1));

    auto changed = candidate();
    changed.windows[0].title = QStringLiteral("Appearance");
    changed.actionGeneration.revision = 9;
    QCOMPARE(store.publish(changed, &error),
             ShellTaskFactsPublishResult::Published);
    QCOMPARE(store.revision(), quint64(2));
}

void ShellTaskFactsTests::roleAndOwnerProvenanceRoundTripsAndRejectsHostileValues()
{
    auto facts = candidate();
    facts.windows[0].type = ShellTaskWindowType::NonNormal;
    facts.windows[0].owner = ShellTaskWindowOwner::BoundShell;
    ShellTaskFactsStore store(QString::fromLatin1(Epoch));
    QString error;
    QCOMPARE(store.publish(facts, &error), ShellTaskFactsPublishResult::Published);
    const auto decoded = decodeShellTaskFactsSnapshot(store.snapshotJson(), &error);
    QVERIFY2(decoded.has_value(), qPrintable(error));
    QCOMPARE(decoded->facts.windows[0].type, ShellTaskWindowType::NonNormal);
    QCOMPARE(decoded->facts.windows[0].owner, ShellTaskWindowOwner::BoundShell);

    QJsonObject root = QJsonDocument::fromJson(store.snapshotJson()).object();
    QJsonArray windows = root.value(QStringLiteral("windows")).toArray();
    QJsonObject hostile = windows[0].toObject();
    hostile.insert(QStringLiteral("ownerRole"), QStringLiteral("untrusted"));
    windows[0] = hostile;
    root.insert(QStringLiteral("windows"), windows);
    QVERIFY(!decodeShellTaskFactsSnapshot(
        QJsonDocument(root).toJson(QJsonDocument::Compact), &error));
}

void ShellTaskFactsTests::hostileBoundsAndReferencesRetainTheGeneration()
{
    ShellTaskFactsStore store(QString::fromLatin1(Epoch));
    QString error;
    QVERIFY(store.publish(candidate(), &error)
            == ShellTaskFactsPublishResult::Published);
    const QByteArray retained = store.snapshotJson();

    auto hostile = candidate();
    hostile.windows[0].title = QString(513, QLatin1Char('x'));
    QCOMPARE(store.publish(hostile, &error),
             ShellTaskFactsPublishResult::Rejected);
    QCOMPARE(store.snapshotJson(), retained);

    hostile = candidate();
    hostile.windows[0].outputId = QStringLiteral("unpublished-output");
    QCOMPARE(store.publish(hostile, &error),
             ShellTaskFactsPublishResult::Rejected);
    QCOMPARE(store.snapshotJson(), retained);

    hostile = candidate();
    hostile.windows.reserve(ShellTaskFactsMaximumWindows + 1);
    while (hostile.windows.size() <= ShellTaskFactsMaximumWindows) {
        auto window = hostile.windows.constFirst();
        window.windowId = QStringLiteral("window-%1").arg(hostile.windows.size());
        hostile.windows.append(std::move(window));
    }
    QCOMPARE(store.publish(hostile, &error),
             ShellTaskFactsPublishResult::Rejected);
    QCOMPARE(store.snapshotJson(), retained);
}

void ShellTaskFactsTests::authenticationPrecedesSourceInspection()
{
    Credentials credentials;
    credentials.pids.insert(QStringLiteral(":1.4"), 42);
    credentials.pids.insert(QStringLiteral(":1.5"), 99);
    PanelOwner panel;
    Source source;
    ShellTaskFactsController controller(credentials, panel, source);

    const QByteArray foreign = controller.snapshot(QStringLiteral(":1.5"));
    const QByteArray malformed = controller.snapshot(QString(1'000'000,
                                                              QLatin1Char('x')));
    QCOMPARE(foreign, malformed);
    QCOMPARE(source.calls, 0);
    const auto denied = decodeShellTaskFactsSnapshot(foreign);
    QVERIFY(denied.has_value());
    QCOMPARE(denied->status, ShellTaskFactsStatus::Unauthorized);

    QCOMPARE(controller.snapshot(QStringLiteral(":1.4")), source.payload);
    QCOMPARE(source.calls, 1);
    panel.pid = 99;
    QCOMPARE(controller.snapshot(QStringLiteral(":1.4")), foreign);
    QCOMPARE(source.calls, 1);
}

QTEST_GUILESS_MAIN(ShellTaskFactsTests)
#include "tst_shelltaskfacts.moc"
