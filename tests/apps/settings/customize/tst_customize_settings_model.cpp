// SPDX-License-Identifier: GPL-3.0-or-later
#include "customize_test_support.h"

#include "qindaqt/apps/settings_customize/customize_editor_host.h"
#include "qindaqt/apps/settings_customize/customize_settings_model.h"
#include "qindaqt/profiles/profile_loader.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/shell_customization/layout_editing_repository.h"
#include "qindaqt/shell_customization/layout_editing_coordinator.h"
#include "qindaqt/shell_customization_editor/user_profile_store.h"
#include "qindaqt/shell_layout/panel_layout_solver.h"

#include <QFileInfo>
#include <QtTest>

#include <algorithm>

using namespace QindaQt::Apps::SettingsCustomize;
using namespace QindaQt::Apps::SettingsCustomize::TestSupport;
using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt;

namespace {

class ModelHarness final {
public:
    ModelHarness()
        : store(temporaryStore(QStringLiteral("customize-model")))
        , client(transport, {QString(LayoutProfileSettingsKey)},
                 {.requestTimeoutMilliseconds = 100,
                  .debounceMilliseconds = 0,
                  .retryMilliseconds = {10}})
        , model(client, {profile(), profile(QStringLiteral("alternate"))},
                manifests(), outputProvider,
                [this](const Profiles::LayoutProfile &selected,
                       const QVector<ShellLayout::LogicalOutput> &inventory) {
                    return std::make_unique<RepositoryCustomizeEditorHost>(
                        selected, inventory, manifests(), store->path());
                })
    {
    }

    bool establish(const QString &profileId = QStringLiteral("fixture"))
    {
        if (!store->isValid() || !client.start()) {
            return false;
        }
        Q_EMIT transport.ownerChanged(QStringLiteral(":1.90"));
        if (!QTest::qWaitFor([this] { return !transport.snapshots.isEmpty(); },
                             5'000)) {
            return false;
        }
        const auto request = transport.snapshots.takeFirst();
        Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                          snapshotWire(profileId));
        return QTest::qWaitFor([this] { return model.ready(); }, 5'000);
    }

    std::unique_ptr<QTemporaryDir> store;
    SequenceTransport transport;
    SettingsClient client;
    MutableCustomizeOutputProvider outputProvider;
    CustomizeSettingsModel model;
};

qsizetype appletCount(const QVariantList &panels)
{
    qsizetype count = 0;
    for (const QVariant &panel : panels) {
        count += panel.toMap().value(QStringLiteral("applets")).toList().size();
    }
    return count;
}

} // namespace

class CustomizeSettingsModelTests final : public QObject {
    Q_OBJECT

private slots:
    void pointerGestureCommitsOneUndoStepAndCancelRollsBack();
    void rejectedTargetRollsBackDeterministically();
    void pointerAndKeyboardPathsConverge();
    void profileProjectionKeepsSelectionInOneLiveProperty();
    void persistenceAndConflictRemainTruthful();
    void foreignLeaseFailsClosedThenRecoversOnRefresh();
    void appliedContentSurvivesDiscardAndAuthorityRecovery();
    void panelDisplayScopeUsesExactPrimaryAndRoundTrips();
    void primaryScopeFailsClosedWithoutStableTruth();
    void primaryChangeFencesDirtyDraft();
};

void CustomizeSettingsModelTests::panelDisplayScopeUsesExactPrimaryAndRoundTrips()
{
    ModelHarness harness;
    QVERIFY(harness.establish());
    harness.model.selectPanel(QStringLiteral("dock"));
    QCOMPARE(harness.model.selectedProperties().value(QStringLiteral("output")),
             QStringLiteral("DP-1"));
    QCOMPARE(harness.model.selectedProperties().value(QStringLiteral("outputScope")),
             QStringLiteral("primary"));
    QVERIFY(harness.model.primaryDisplayAvailable());

    const auto primarySolved = ShellLayout::PanelLayoutSolver::solve(
        profile().panels, outputs());
    QVERIFY2(primarySolved.ok(), qPrintable(primarySolved.error.message));
    QStringList primaryDockOutputs;
    for (const auto &surface : primarySolved.surfaces) {
        if (surface.panelId == QLatin1String("dock")) {
            primaryDockOutputs.append(surface.outputId);
        }
    }
    QCOMPARE(primaryDockOutputs, QStringList({QStringLiteral("DP-1")}));

    QVERIFY(harness.model.configureSelectedPanel(QStringLiteral("outputScope"),
                                                 QStringLiteral("all")));
    QCOMPARE(harness.model.selectedProperties().value(QStringLiteral("output")),
             QStringLiteral("*"));
    QVERIFY(harness.model.canUndo());
    QVERIFY(harness.model.undo());
    QCOMPARE(harness.model.selectedProperties().value(QStringLiteral("output")),
             QStringLiteral("DP-1"));
    QVERIFY(!harness.model.canUndo());
    QVERIFY(harness.model.redo());
    QVERIFY(harness.model.apply());

    const QString saved = QDir(harness.store->path()).filePath(
        ShellCustomizationEditor::UserProfileStore::fileNameForId(
            QStringLiteral("fixture")));
    const Profiles::LoadResult loaded = Profiles::ProfileLoader::fromFile(saved);
    QVERIFY2(loaded.ok, qPrintable(loaded.error.message));
    const auto dock = std::find_if(
        loaded.profile.panels.cbegin(), loaded.profile.panels.cend(),
        [](const Profiles::PanelSpec &panel) {
            return panel.id == QLatin1String("dock");
        });
    QVERIFY(dock != loaded.profile.panels.cend());
    QCOMPARE(dock->output, QStringLiteral("*"));

    const auto solved = ShellLayout::PanelLayoutSolver::solve(
        loaded.profile.panels, outputs());
    QVERIFY2(solved.ok(), qPrintable(solved.error.message));
    QStringList dockOutputs;
    for (const auto &surface : solved.surfaces) {
        if (surface.panelId == QLatin1String("dock")) {
            dockOutputs.append(surface.outputId);
        }
    }
    QCOMPARE(dockOutputs,
             QStringList({QStringLiteral("DP-1"),
                          QStringLiteral("HDMI-A-1")}));
}

void CustomizeSettingsModelTests::primaryScopeFailsClosedWithoutStableTruth()
{
    ModelHarness harness;
    CustomizeOutputSnapshot missing = harness.outputProvider.snapshot();
    missing.primaryOutputIds.clear();
    ++missing.revision;
    harness.outputProvider.publish(missing);
    QVERIFY(harness.establish());
    harness.model.selectPanel(QStringLiteral("dock"));
    const QVariantList before = harness.model.panels();
    QVERIFY(!harness.model.primaryDisplayAvailable());
    QVERIFY(harness.model.displayScopeError().contains(
        QStringLiteral("not currently known")));
    QVERIFY(!harness.model.configureSelectedPanel(QStringLiteral("outputScope"),
                                                  QStringLiteral("primary")));
    QCOMPARE(harness.model.panels(), before);
    QVERIFY(!harness.model.canUndo());

    CustomizeOutputSnapshot ambiguous = missing;
    ambiguous.primaryOutputIds = {QStringLiteral("DP-1"),
                                  QStringLiteral("HDMI-A-1")};
    ++ambiguous.revision;
    harness.outputProvider.publish(ambiguous);
    QVERIFY(!harness.model.primaryDisplayAvailable());
    QVERIFY(harness.model.displayScopeError().contains(
        QStringLiteral("More than one")));
    QVERIFY(!harness.model.configureSelectedPanel(QStringLiteral("outputScope"),
                                                  QStringLiteral("primary")));
    QCOMPARE(harness.model.panels(), before);
}

void CustomizeSettingsModelTests::primaryChangeFencesDirtyDraft()
{
    ModelHarness harness;
    QVERIFY(harness.establish());
    harness.model.selectPanel(QStringLiteral("dock"));
    QVERIFY(harness.model.configureSelectedPanel(QStringLiteral("outputScope"),
                                                 QStringLiteral("all")));

    CustomizeOutputSnapshot changed = harness.outputProvider.snapshot();
    changed.primaryOutputIds = {QStringLiteral("HDMI-A-1")};
    ++changed.revision;
    harness.outputProvider.publish(changed);

    QVERIFY(!harness.model.primaryDisplayAvailable());
    QVERIFY(harness.model.displayScopeError().contains(
        QStringLiteral("changed while editing")));
    QVERIFY(!harness.model.applyAvailable());
    QVERIFY(!harness.model.configureSelectedPanel(QStringLiteral("outputScope"),
                                                  QStringLiteral("primary")));
    QCOMPARE(harness.model.selectedProperties().value(QStringLiteral("output")),
             QStringLiteral("*"));
    QVERIFY(!harness.model.apply());
    const QString saved = QDir(harness.store->path()).filePath(
        ShellCustomizationEditor::UserProfileStore::fileNameForId(
            QStringLiteral("fixture")));
    QVERIFY(!QFileInfo::exists(saved));

    QVERIFY(harness.model.discard());
    QVERIFY(harness.model.primaryDisplayAvailable());
    QVERIFY(harness.model.configureSelectedPanel(QStringLiteral("outputScope"),
                                                 QStringLiteral("primary")));
    QCOMPARE(harness.model.selectedProperties().value(QStringLiteral("output")),
             QStringLiteral("HDMI-A-1"));
}

void CustomizeSettingsModelTests::pointerGestureCommitsOneUndoStepAndCancelRollsBack()
{
    ModelHarness harness;
    QVERIFY(harness.establish());
    const qsizetype baseline = appletCount(harness.model.panels());

    QVERIFY(harness.model.startPaletteDrag(QStringLiteral("clock")));
    QVERIFY(harness.model.hoverDropTarget(QStringLiteral("dock"),
                                          QStringLiteral("start")));
    QVERIFY(harness.model.dropAccepted());
    QVERIFY(harness.model.commitDrag());
    QCOMPARE(appletCount(harness.model.panels()), baseline + 1);
    QVERIFY(harness.model.dirty());
    QVERIFY(harness.model.canUndo());

    QVERIFY(harness.model.undo());
    QCOMPARE(appletCount(harness.model.panels()), baseline);
    QVERIFY(!harness.model.canUndo());
    QVERIFY(harness.model.canRedo());
    QVERIFY(harness.model.redo());
    QCOMPARE(appletCount(harness.model.panels()), baseline + 1);

    const QVariantList beforeCancel = harness.model.panels();
    QVERIFY(harness.model.startAppletDrag(QStringLiteral("bar"),
                                          QStringLiteral("clock-instance")));
    QVERIFY(harness.model.hoverDropTarget(QStringLiteral("dock"),
                                          QStringLiteral("end")));
    QVERIFY(harness.model.cancelDrag());
    QCOMPARE(harness.model.panels(), beforeCancel);
}

void CustomizeSettingsModelTests::rejectedTargetRollsBackDeterministically()
{
    ModelHarness harness;
    QVERIFY(harness.establish());
    const QVariantList baseline = harness.model.panels();

    QVERIFY(harness.model.startPaletteDrag(QStringLiteral("launcher")));
    QVERIFY(harness.model.hoverDropTarget(QStringLiteral("missing-panel"),
                                          QStringLiteral("start")));
    QVERIFY(!harness.model.dropAccepted());
    QVERIFY(!harness.model.dropReason().isEmpty());
    QVERIFY(harness.model.cancelDrag());
    QCOMPARE(harness.model.panels(), baseline);
    QVERIFY(!harness.model.dirty());
}

void CustomizeSettingsModelTests::pointerAndKeyboardPathsConverge()
{
    ModelHarness pointer;
    ModelHarness keyboard;
    QVERIFY(pointer.establish());
    QVERIFY(keyboard.establish());

    QVERIFY(pointer.model.startPaletteDrag(QStringLiteral("launcher")));
    QVERIFY(pointer.model.hoverDropTarget(QStringLiteral("dock"),
                                          QStringLiteral("start")));
    QVERIFY(pointer.model.commitDrag());
    QVERIFY(keyboard.model.keyboardInsert(QStringLiteral("launcher"),
                                          QStringLiteral("dock"),
                                          QStringLiteral("start")));
    QCOMPARE(pointer.model.panels(), keyboard.model.panels());
}

void CustomizeSettingsModelTests::profileProjectionKeepsSelectionInOneLiveProperty()
{
    ModelHarness harness;
    QVERIFY(harness.establish());
    for (const QVariant &profile : harness.model.profiles()) {
        QVERIFY(!profile.toMap().contains(QStringLiteral("selected")));
    }

    QVERIFY(harness.model.selectProfile(QStringLiteral("alternate")));
    QCOMPARE(harness.model.selectedProfileId(), QStringLiteral("alternate"));
}

void CustomizeSettingsModelTests::persistenceAndConflictRemainTruthful()
{
    ModelHarness harness;
    QVERIFY(harness.establish());
    QVERIFY(harness.model.keyboardInsert(QStringLiteral("clock"),
                                         QStringLiteral("dock"),
                                         QStringLiteral("start")));
    QVERIFY(harness.model.apply());
    QVERIFY(!harness.model.dirty());
    QCOMPARE(harness.model.statusText(),
             QStringLiteral(
                 "The running desktop follows each applied layout change"));
    const QString saved = QDir(harness.store->path()).filePath(
        ShellCustomizationEditor::UserProfileStore::fileNameForId(
            QStringLiteral("fixture")));
    QVERIFY2(QFileInfo::exists(saved), qPrintable(saved));

    QVERIFY(harness.model.selectProfile(QStringLiteral("alternate")));
    QVERIFY(harness.model.apply());
    QVERIFY(harness.model.saving());
    QCOMPARE(harness.transport.commits.size(), 1);
    const auto commit = harness.transport.commits.constFirst();
    Q_EMIT harness.transport.commitReceived(
        commit.token, commit.owner,
        commitWire(Services::SettingsProtocol::SettingsWireStatus::Conflict,
                   QStringLiteral("fixture"),
                   QStringLiteral("changed elsewhere")));
    QTRY_VERIFY(harness.model.conflict());
    QVERIFY(harness.model.dirty());
    QVERIFY(harness.model.errorText().contains(QStringLiteral("changed elsewhere")));
}

void CustomizeSettingsModelTests::appliedContentSurvivesDiscardAndAuthorityRecovery()
{
    ModelHarness harness;
    QVERIFY(harness.establish());
    QVERIFY(harness.model.keyboardInsert(QStringLiteral("clock"),
                                         QStringLiteral("dock"), QStringLiteral("start")));
    QVERIFY(harness.model.apply());
    const auto applied = harness.model.panels();
    QVERIFY(harness.model.keyboardInsert(QStringLiteral("clock"),
                                         QStringLiteral("dock"), QStringLiteral("end")));
    QVERIFY(harness.model.discard());
    QCOMPARE(harness.model.panels(), applied);
    QVERIFY(!harness.model.dirty());

    Q_EMIT harness.transport.ownerChanged(QString{});
    QTRY_VERIFY(harness.model.unavailable());
    Q_EMIT harness.transport.ownerChanged(QStringLiteral(":1.91"));
    QTRY_VERIFY(!harness.transport.snapshots.isEmpty());
    const auto request = harness.transport.snapshots.takeFirst();
    Q_EMIT harness.transport.snapshotReceived(request.token, request.owner,
                                              snapshotWire(QStringLiteral("fixture")));
    QTRY_VERIFY(harness.model.ready());
    QCOMPARE(harness.model.panels(), applied);
    QVERIFY(!harness.model.dirty());

    // A failed selection commit cannot undo content already stored for a
    // different profile. Re-selecting it must use its successfully saved bytes.
    QVERIFY(harness.model.selectProfile(QStringLiteral("alternate")));
    QVERIFY(harness.model.keyboardInsert(QStringLiteral("clock"),
                                         QStringLiteral("dock"), QStringLiteral("start")));
    QVERIFY(harness.model.apply());
    const auto alternateApplied = harness.model.panels();
    const auto commit = harness.transport.commits.constLast();
    Q_EMIT harness.transport.commitReceived(commit.token, commit.owner,
        commitWire(Services::SettingsProtocol::SettingsWireStatus::Conflict,
                   QStringLiteral("fixture"), QStringLiteral("changed elsewhere")));
    QTRY_VERIFY(harness.model.conflict());
    QTRY_VERIFY(!harness.transport.snapshots.isEmpty());
    const auto refresh = harness.transport.snapshots.takeLast();
    Q_EMIT harness.transport.snapshotReceived(refresh.token, refresh.owner,
        snapshotWire(QStringLiteral("fixture"), QStringLiteral("epoch-a"), 8));
    QTRY_VERIFY(harness.model.canEdit());
    QVERIFY(harness.model.discard());
    QCOMPARE(harness.model.panels(), applied);
    QVERIFY(harness.model.selectProfile(QStringLiteral("alternate")));
    QCOMPARE(harness.model.panels(), alternateApplied);
}

void CustomizeSettingsModelTests::foreignLeaseFailsClosedThenRecoversOnRefresh()
{
    auto store = temporaryStore(QStringLiteral("customize-lease"));
    QVERIFY(store->isValid());
    ShellCustomization::LayoutEditingRepository repository(
        profile(), outputs(), manifests());
    auto foreign = repository.tryAcquireCoordinator();
    QVERIFY(foreign != nullptr);

    SequenceTransport transport;
    SettingsClient client(transport, {QString(LayoutProfileSettingsKey)},
                          {.requestTimeoutMilliseconds = 100,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    MutableCustomizeOutputProvider outputProvider;
    CustomizeSettingsModel model(
        client, {profile()}, manifests(), outputProvider,
        [&repository, &store](const Profiles::LayoutProfile &,
                              const QVector<ShellLayout::LogicalOutput> &) {
            return std::make_unique<RepositoryCustomizeEditorHost>(
                repository, manifests(), store->path());
        });
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.91"));
    QTRY_VERIFY(!transport.snapshots.isEmpty());
    auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(QStringLiteral("fixture")));
    QTRY_VERIFY(model.unavailable());
    QVERIFY(model.errorText().contains(QStringLiteral("another window")));
    QVERIFY(!model.keyboardInsert(QStringLiteral("clock"),
                                  QStringLiteral("dock"),
                                  QStringLiteral("start")));

    foreign.reset();
    model.retry();
    QTRY_VERIFY(!transport.snapshots.isEmpty());
    request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(QStringLiteral("fixture"),
                                                   QStringLiteral("epoch-a"), 8));
    QTRY_VERIFY(model.ready());
    QVERIFY(model.canEdit());
}

QTEST_GUILESS_MAIN(CustomizeSettingsModelTests)
#include "tst_customize_settings_model.moc"
