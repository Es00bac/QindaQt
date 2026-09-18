// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_test_fixtures.h"

#include "qindaqt/profiles/profile_loader.h"
#include "qindaqt/shell_customization/layout_editing_repository.h"
#include "qindaqt/shell_customization_editor/coordinator_engine_adapter.h"
#include "qindaqt/shell_customization_editor/editor_session.h"
#include "qindaqt/shell_customization_editor/intent_translator.h"
#include "qindaqt/shell_customization_editor/live_editor_host.h"
#include "qindaqt/shell_customization_editor/user_profile_store.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <functional>
#include <memory>

using namespace QindaQt;
using namespace QindaQt::ShellCustomizationEditor;
using namespace QindaQt::ShellCustomizationEditor::TestFixtures;
using namespace QindaQt::ShellCustomization;

namespace {

QByteArray readProfileBytes(const QString &directory, const QString &profileId)
{
    QFile file(QDir(directory).filePath(UserProfileStore::fileNameForId(profileId)));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

DropTarget target(const QString &panelId, const QString &zone,
                  const QString &beforeAppletId = {})
{
    DropTarget result;
    result.panelId = panelId;
    result.zone = zone;
    if (!beforeAppletId.isEmpty()) {
        result.beforeAppletId = beforeAppletId;
    }
    return result;
}

Profiles::PanelSpec newPanel(const QString &id)
{
    Profiles::PanelSpec panel;
    panel.id = id;
    panel.output = QStringLiteral("*");
    panel.edge = Profiles::Edge::Left;
    panel.thickness = 40;
    return panel;
}

// The same live-customization intent script, run through any host-like
// callable so byte parity can be asserted between two compositions.
using Step = std::function<EditorOutcome()>;

template <typename Host>
QVector<Step> script(Host &host)
{
    return {
        [&host] { return host.applyGesture(removeIntent(QStringLiteral("bar"), QStringLiteral("clock-instance")),
                                           target(QStringLiteral("bar"), QStringLiteral("start"))); },
        [&host] { return host.applyGesture(InsertAppletIntent{QStringLiteral("clock")},
                                           target(QStringLiteral("dock"), QStringLiteral("end")),
                                           QStringLiteral("clock-live-1")); },
        // A cross-panel zone move: the flat move plus its zone companion.
        [&host] { return host.applyGesture(MoveAppletIntent{QStringLiteral("dock"), QStringLiteral("tasks-instance")},
                                           target(QStringLiteral("bar"), QStringLiteral("center"))); },
        [&host] { return host.applyGesture(addPanelIntent(newPanel(QStringLiteral("side")), std::nullopt),
                                           target(QStringLiteral("side"), QStringLiteral("start"))); },
        [&host] { return host.applyGesture(configureIntent(QStringLiteral("dock"),
                                                           PanelConfiguration{Profiles::Layer::Overlay,
                                                                              Profiles::HideMode::Intelligent,
                                                                              1, 56, 0.6}),
                                           target(QStringLiteral("dock"), QStringLiteral("start"))); },
        [&host] { return host.undo(); },
        [&host] { return host.applyGesture(removePanelIntent(QStringLiteral("side")),
                                           target(QStringLiteral("side"), QStringLiteral("start"))); },
        [&host] { return host.apply(); },
    };
}

// A hand-built copy of the Settings route's composition (see
// customize_editor_host.cpp): the reference the live host must match.
class ReferenceComposition final {
public:
    ReferenceComposition(const QString &directory)
        : repository(profile(), outputs(), manifests())
        , engine(repository, manifests())
        , session(engine, UserProfileStore(directory))
    {
    }

    EditorOutcome applyGesture(const CustomizationIntent &intent, const DropTarget &at,
                               const QString &newId = {})
    {
        return session.applyGesture(intent, at, newId);
    }
    EditorOutcome undo() { return session.undo(); }
    EditorOutcome apply() { return session.applyToUserProfile(); }

    LayoutEditingRepository repository;
    CoordinatorEditingEngine engine;
    EditorSession session;
};

} // namespace

class LiveEditorHostTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void composesReadyHostOverTheFixture();
    void panelIntentsTranslateToPanelCommands();
    void panelIntentsAreStructurallyValidated();
    void appliesOneUndoStepPerMenuActionAndPersists();
    void producesTheSameBytesAsTheSettingsRouteComposition();
    void rebuildKeepsManifestsAndStoreButDropsHistory();
};

void LiveEditorHostTest::composesReadyHostOverTheFixture()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LiveEditorHost host(profile(), outputs(), manifests(), directory.path());
    QVERIFY2(host.ready(), qPrintable(host.unavailableReason()));
    QVERIFY(host.unavailableReason().isEmpty());
    QVERIFY(!host.requiresRebuild());
    QVERIFY(host.profile() != nullptr);
    QCOMPARE(host.profile()->id, QStringLiteral("editor-fixture"));
    QVERIFY(host.layout() != nullptr);
    QCOMPARE(host.manifests().size(), 3);
    QCOMPARE(host.outputs().size(), 2);
    QCOMPARE(host.userProfileDirectory(), directory.path());
    QVERIFY(!host.dirty());
    QVERIFY(!host.canUndo());
    QVERIFY(!host.visualDragActive());
    const auto committed = host.committedProfile();
    QVERIFY(committed != nullptr);
    QCOMPARE(committed->panels.size(), 2);
}

void LiveEditorHostTest::panelIntentsTranslateToPanelCommands()
{
    TranslationContext context;
    context.expectedRevision = 9;
    const auto added = translateIntent(addPanelIntent(newPanel(QStringLiteral("side")),
                                                      QStringLiteral("dock")),
                                       target(QStringLiteral("side"), QStringLiteral("start")),
                                       context);
    QCOMPARE(added.size(), 1);
    QCOMPARE(commandKind(added.first()), EditingCommandKind::AddPanel);
    const auto &addCommand = std::get<AddPanelCommand>(added.first());
    QCOMPARE(addCommand.expectedRevision, quint64{9});
    QCOMPARE(addCommand.panel.id, QStringLiteral("side"));
    QVERIFY(addCommand.panel.edge == Profiles::Edge::Left);
    QVERIFY(addCommand.beforePanelId.has_value());
    QCOMPARE(*addCommand.beforePanelId, QStringLiteral("dock"));

    const auto removed = translateIntent(removePanelIntent(QStringLiteral("dock")),
                                         target(QStringLiteral("dock"), QStringLiteral("start")),
                                         context);
    QCOMPARE(removed.size(), 1);
    QCOMPARE(commandKind(removed.first()), EditingCommandKind::RemovePanel);
    QCOMPARE(std::get<RemovePanelCommand>(removed.first()).panelId, QStringLiteral("dock"));

    // The bracketed form stays Begin/mutation/Commit, one durable undo step.
    const auto sequence = gestureSequence(removePanelIntent(QStringLiteral("dock")),
                                          target(QStringLiteral("dock"), QStringLiteral("start")),
                                          context);
    QCOMPARE(sequence.size(), 3);
    QCOMPARE(commandKind(sequence.first()), EditingCommandKind::BeginPreview);
    QCOMPARE(commandKind(sequence.last()), EditingCommandKind::CommitPreview);
    QVERIFY(intentKind(removePanelIntent(QStringLiteral("dock"))) == IntentKind::RemovePanel);
    QVERIFY(intentKind(addPanelIntent(newPanel(QStringLiteral("x")), std::nullopt)) == IntentKind::AddPanel);
}

void LiveEditorHostTest::panelIntentsAreStructurallyValidated()
{
    const DropTarget at = target(QStringLiteral("side"), QStringLiteral("start"));
    QCOMPARE(validateIntent(addPanelIntent(newPanel(QString()), std::nullopt), at).code,
             IntentErrorCode::EmptyPanelId);
    QCOMPARE(validateIntent(addPanelIntent(newPanel(QStringLiteral("side")), QStringLiteral("side")), at).code,
             IntentErrorCode::AnchorSelfReference);
    Profiles::PanelSpec tooLong = newPanel(QStringLiteral("side"));
    tooLong.length = 1.5;
    QCOMPARE(validateIntent(addPanelIntent(tooLong, std::nullopt), at).code,
             IntentErrorCode::InvalidConfiguration);
    Profiles::PanelSpec zeroRows = newPanel(QStringLiteral("side"));
    zeroRows.rows = 0;
    QCOMPARE(validateIntent(addPanelIntent(zeroRows, std::nullopt), at).code,
             IntentErrorCode::InvalidConfiguration);
    QVERIFY(validateIntent(addPanelIntent(newPanel(QStringLiteral("side")), std::nullopt), at).ok());
    QCOMPARE(validateIntent(removePanelIntent(QString()), at).code, IntentErrorCode::EmptyPanelId);
    QVERIFY(validateIntent(removePanelIntent(QStringLiteral("dock")), at).ok());
}

void LiveEditorHostTest::appliesOneUndoStepPerMenuActionAndPersists()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LiveEditorHost host(profile(), outputs(), manifests(), directory.path());
    QVERIFY(host.ready());

    // Remove "clock" from the bar: one gesture, one undo step, dirty.
    auto outcome = host.applyGesture(removeIntent(QStringLiteral("bar"), QStringLiteral("clock-instance")),
                                     target(QStringLiteral("bar"), QStringLiteral("start")));
    QVERIFY2(outcome.ok, qPrintable(outcome.message));
    QVERIFY(host.dirty());
    QVERIFY(host.canUndo());
    QCOMPARE(appletIds(*host.profile(), QStringLiteral("bar")),
             QStringList{QStringLiteral("launcher-instance")});

    // Apply persists the committed profile and clears dirty; history stays.
    outcome = host.apply();
    QVERIFY2(outcome.ok, qPrintable(outcome.message));
    QVERIFY(!host.dirty());
    QVERIFY(host.canUndo());
    const QByteArray bytes = readProfileBytes(directory.path(), QStringLiteral("editor-fixture"));
    QVERIFY(!bytes.isEmpty());
    const auto loaded = Profiles::ProfileLoader::fromJson(bytes, QStringLiteral("test"));
    QVERIFY2(loaded.ok, qPrintable(loaded.error.message));
    QCOMPARE(appletIds(loaded.profile, QStringLiteral("bar")),
             QStringList{QStringLiteral("launcher-instance")});

    // Add a panel through the engine's real AddPanel path, then remove it.
    outcome = host.applyGesture(addPanelIntent(newPanel(QStringLiteral("side")), std::nullopt),
                                target(QStringLiteral("side"), QStringLiteral("start")));
    QVERIFY2(outcome.ok, qPrintable(outcome.message));
    QCOMPARE(host.profile()->panels.size(), 3);
    QVERIFY(panel(*host.profile(), QStringLiteral("side")) != nullptr);
    outcome = host.applyGesture(removePanelIntent(QStringLiteral("side")),
                                target(QStringLiteral("side"), QStringLiteral("start")));
    QVERIFY2(outcome.ok, qPrintable(outcome.message));
    QCOMPARE(host.profile()->panels.size(), 2);

    // Undo the removal: one step brings the panel back.
    outcome = host.undo();
    QVERIFY2(outcome.ok, qPrintable(outcome.message));
    QCOMPARE(host.profile()->panels.size(), 3);
    QVERIFY(host.canRedo());
    outcome = host.redo();
    QVERIFY2(outcome.ok, qPrintable(outcome.message));
    QCOMPARE(host.profile()->panels.size(), 2);

    // A structurally invalid menu action never reaches the engine.
    outcome = host.applyGesture(removePanelIntent(QString()),
                                target(QStringLiteral("bar"), QStringLiteral("start")));
    QVERIFY(!outcome.ok);
    QCOMPARE(outcome.code, EditorErrorCode::IntentInvalid);
    // An unknown panel is rejected by the engine with a typed failure.
    outcome = host.applyGesture(removePanelIntent(QStringLiteral("missing")),
                                target(QStringLiteral("missing"), QStringLiteral("start")));
    QVERIFY(!outcome.ok);
    QCOMPARE(outcome.code, EditorErrorCode::CommandFailed);
    QCOMPARE(host.profile()->panels.size(), 2);
}

void LiveEditorHostTest::producesTheSameBytesAsTheSettingsRouteComposition()
{
    QTemporaryDir liveDirectory;
    QTemporaryDir referenceDirectory;
    QVERIFY(liveDirectory.isValid() && referenceDirectory.isValid());
    LiveEditorHost live(profile(), outputs(), manifests(), liveDirectory.path());
    ReferenceComposition reference(referenceDirectory.path());

    const auto liveSteps = script(live);
    const auto referenceSteps = script(reference);
    QCOMPARE(liveSteps.size(), referenceSteps.size());
    for (qsizetype index = 0; index < liveSteps.size(); ++index) {
        const EditorOutcome fromLive = liveSteps.at(index)();
        const EditorOutcome fromReference = referenceSteps.at(index)();
        QVERIFY2(fromLive.ok, qPrintable(QStringLiteral("live step %1: %2").arg(index).arg(fromLive.message)));
        QVERIFY2(fromReference.ok, qPrintable(QStringLiteral("reference step %1: %2").arg(index).arg(fromReference.message)));
    }
    const QByteArray liveBytes = readProfileBytes(liveDirectory.path(), QStringLiteral("editor-fixture"));
    const QByteArray referenceBytes = readProfileBytes(referenceDirectory.path(), QStringLiteral("editor-fixture"));
    QVERIFY(!liveBytes.isEmpty());
    QCOMPARE(liveBytes, referenceBytes);
    // Sanity on the content the script produced: a new clock sits in the dock
    // end zone, the task list moved to the bar center, the side panel came and
    // went, and the undone dock configuration is not persisted.
    const auto loaded = Profiles::ProfileLoader::fromJson(liveBytes, QStringLiteral("live"));
    QVERIFY2(loaded.ok, qPrintable(loaded.error.message));
    QCOMPARE(loaded.profile.panels.size(), 2);
    QCOMPARE(appletIds(loaded.profile, QStringLiteral("dock")), QStringList{QStringLiteral("clock-live-1")});
    QCOMPARE(panel(loaded.profile, QStringLiteral("dock"))->applets.last().settings.value(QStringLiteral("zone")).toString(),
             QStringLiteral("end"));
    QCOMPARE(appletIds(loaded.profile, QStringLiteral("bar")),
             (QStringList{QStringLiteral("launcher-instance"), QStringLiteral("tasks-instance")}));
    QCOMPARE(panel(loaded.profile, QStringLiteral("bar"))->applets.last().settings.value(QStringLiteral("zone")).toString(),
             QStringLiteral("center"));
    QCOMPARE(panel(loaded.profile, QStringLiteral("dock"))->thickness, 48);
}

void LiveEditorHostTest::rebuildKeepsManifestsAndStoreButDropsHistory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LiveEditorHost host(profile(), outputs(), manifests(), directory.path());
    auto outcome = host.applyGesture(removeIntent(QStringLiteral("bar"), QStringLiteral("clock-instance")),
                                     target(QStringLiteral("bar"), QStringLiteral("start")));
    QVERIFY(outcome.ok);
    QVERIFY(host.canUndo());

    // An output-generation change stales the session until a rebuild.
    outcome = host.notifyOutputGenerationChanged();
    QVERIFY(!outcome.ok);
    QCOMPARE(outcome.code, EditorErrorCode::SessionStale);
    QVERIFY(host.requiresRebuild());
    outcome = host.applyGesture(removeIntent(QStringLiteral("bar"), QStringLiteral("launcher-instance")),
                                target(QStringLiteral("bar"), QStringLiteral("start")));
    QVERIFY(!outcome.ok);

    host.rebuild(profile(), outputs().mid(1));
    QVERIFY2(host.ready(), qPrintable(host.unavailableReason()));
    QVERIFY(!host.requiresRebuild());
    QVERIFY(!host.canUndo());
    QVERIFY(!host.dirty());
    QCOMPARE(host.outputs().size(), 1);
    QCOMPARE(host.manifests().size(), 3);
    QCOMPARE(host.userProfileDirectory(), directory.path());
    QCOMPARE(appletIds(*host.profile(), QStringLiteral("bar")),
             (QStringList{QStringLiteral("launcher-instance"), QStringLiteral("clock-instance")}));
}

QTEST_GUILESS_MAIN(LiveEditorHostTest)
#include "tst_live_editor_host.moc"
