// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_test_fixtures.h"

#include "qindaqt/shell_customization/editing_commands.h"
#include "qindaqt/shell_customization/layout_editing_coordinator.h"
#include "qindaqt/shell_customization/layout_editing_repository.h"
#include "qindaqt/shell_customization_editor/coordinator_engine_adapter.h"
#include "qindaqt/shell_customization_editor/editor_session.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest/QtTest>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

using namespace QindaQt;
using namespace QindaQt::ShellCustomizationEditor;
using namespace QindaQt::ShellCustomizationEditor::TestFixtures;

// The editor domain consumes the transaction engine public vocabulary
// throughout; the sibling namespace is imported file-locally per convention.
using namespace QindaQt::ShellCustomization;

namespace {

using CommandKind = QindaQt::ShellCustomization::EditingCommandKind;

// Scripted engine: advances the revision on every successful execute and can
// be told to fail the next occurrence of a command kind, which is enough to
// drive every deterministic rollback path of the session.
class ScriptedEngine final : public EditingEngine
{
public:
    explicit ScriptedEngine(Profiles::LayoutProfile profile)
        : m_profile(std::move(profile))
    {
    }

    EditingResult execute(const QindaQt::ShellCustomization::EditingCommand &command) override
    {
        EditingResult result;
        result.kind = QindaQt::ShellCustomization::commandKind(command);
        result.previousRevision = m_revision;
        const quint64 requested = QindaQt::ShellCustomization::expectedRevision(command);
        if (requested != m_revision) {
            result.error.code = QindaQt::ShellCustomization::EditingErrorCode::StaleRevision;
            result.error.message = QStringLiteral("stale revision");
            result.revision = m_revision;
            return result;
        }
        const auto scripted = std::find(m_failures.begin(), m_failures.end(), result.kind);
        if (scripted != m_failures.end()) {
            m_failures.erase(scripted);
            result.error.code = QindaQt::ShellCustomization::EditingErrorCode::InvalidCommand;
            result.error.message = QStringLiteral("scripted failure");
            result.revision = m_revision;
            return result;
        }
        m_revision = m_revision + 1;
        result.revision = m_revision;
        m_executed.push_back(result.kind);
        if (result.kind == CommandKind::BeginPreview) {
            m_previewActive = true;
        } else if (result.kind == CommandKind::CommitPreview
                   || result.kind == CommandKind::CancelPreview) {
            m_previewActive = false;
        }
        return result;
    }

    QindaQt::ShellCustomization::EditingEvaluation
    evaluate(const QindaQt::ShellCustomization::EditingCommand &command) const override
    {
        QindaQt::ShellCustomization::EditingEvaluation evaluation;
        evaluation.kind = QindaQt::ShellCustomization::commandKind(command);
        evaluation.revision = m_revision;
        if (!m_evaluationAccepted) {
            evaluation.error.code =
                QindaQt::ShellCustomization::EditingErrorCode::UnsupportedAppletPlacement;
            evaluation.error.message = QStringLiteral("scripted rejection");
        }
        return evaluation;
    }

    SequenceEvaluation evaluateSequence(
        const QVector<QindaQt::ShellCustomization::EditingCommand> &commands) const override
    {
        SequenceEvaluation evaluation;
        evaluation.revision = m_revision;
        evaluation.accepted = !commands.isEmpty() && m_evaluationAccepted;
        if (!evaluation.accepted) {
            evaluation.error.code =
                QindaQt::ShellCustomization::EditingErrorCode::UnsupportedAppletPlacement;
            evaluation.error.message = QStringLiteral("scripted rejection");
        }
        return evaluation;
    }

    std::shared_ptr<const QindaQt::ShellCustomization::LayoutEditingSnapshot> snapshot() const override
    {
        auto current = std::make_shared<
            QindaQt::ShellCustomization::LayoutEditingSnapshot>();
        current->profile = m_profile;
        current->revision = m_revision;
        current->previewActive = m_previewActive;
        return current;
    }

    QindaQt::ShellCustomization::LayoutEditingStatus status() const override
    {
        QindaQt::ShellCustomization::LayoutEditingStatus value;
        value.previewActive = m_previewActive;
        value.canUndo = !m_previewActive && m_revision > 0;
        value.canRedo = false;
        return value;
    }

    bool hasPreview() const override { return m_previewActive; }

    void failNext(CommandKind kind) { m_failures.push_back(kind); }
    void setEvaluationAccepted(bool accepted) { m_evaluationAccepted = accepted; }

    [[nodiscard]] std::vector<CommandKind> executed() const { return m_executed; }
    [[nodiscard]] quint64 revision() const { return m_revision; }

private:
    Profiles::LayoutProfile m_profile;
    quint64 m_revision = 4;
    bool m_previewActive = false;
    bool m_evaluationAccepted = true;
    std::vector<CommandKind> m_failures;
    std::vector<CommandKind> m_executed;
};

class EditorSessionTest final : public QObject
{
    Q_OBJECT

private slots:
    void gestureCommitProducesOneBracketedSequence()
    {
        ScriptedEngine engine(profile());
        EditorSession session(engine, UserProfileStore{QStringLiteral("/unused")});

        QVERIFY(session.armDrag(palettePayload(QStringLiteral("clock"))).ok);
        QVERIFY(session.beginVisualDrag().ok);

        const DropTarget target{QStringLiteral("bar"), QStringLiteral("start"),
                                QStringLiteral("launcher-instance")};
        QVERIFY(session.hoverTarget(target).ok);

        // Palette insert moved to the first accepted target: BeginPreview +
        // InsertApplet have executed; CommitPreview closes on drop.
        const auto executed = engine.executed();
        QCOMPARE(executed.size(), size_t{2});
        QCOMPARE(executed.at(0), CommandKind::BeginPreview);
        QCOMPARE(executed.at(1), CommandKind::InsertApplet);

        QVERIFY(session.drop().ok);
        QCOMPARE(engine.executed().back(), CommandKind::CommitPreview);
        QVERIFY(session.isDirty());
        QCOMPARE(session.gestureState(), GestureState::Idle);
        QVERIFY(!session.isVisualDragActive());
    }

    void hoverRejectionExecutesNothing()
    {
        ScriptedEngine engine(profile());
        engine.setEvaluationAccepted(false);
        EditorSession session(engine, UserProfileStore{QStringLiteral("/unused")});

        QVERIFY(session.armDrag(instancePayload(QStringLiteral("bar"),
                                                QStringLiteral("clock-instance")))
                    .ok);
        QVERIFY(session.beginVisualDrag().ok);

        const DropTarget target{QStringLiteral("bar"), QStringLiteral("start"),
                                QStringLiteral("launcher-instance")};
        QVERIFY(session.hoverTarget(target).ok);

        QVERIFY(session.acceptance().has_value());
        QVERIFY(!session.acceptance()->accepted);
        QVERIFY(!session.acceptance()->reason.isEmpty());
        // Only BeginPreview ran; a rejected target never executes.
        QCOMPARE(engine.executed().size(), size_t{1});
    }

    void failedInDragCommandRollsTheGestureBack()
    {
        ScriptedEngine engine(profile());
        engine.failNext(CommandKind::UpdateAppletSettings);
        EditorSession session(engine, UserProfileStore{QStringLiteral("/unused")});

        QVERIFY(session.armDrag(instancePayload(QStringLiteral("bar"),
                                                QStringLiteral("clock-instance")))
                    .ok);
        QVERIFY(session.beginVisualDrag().ok);

        // Zone-crossing move: MoveApplet succeeds, the scripted
        // UpdateAppletSettings failure must close the bracket via cancel.
        const DropTarget target{QStringLiteral("bar"), QStringLiteral("start"),
                                QStringLiteral("launcher-instance")};
        const EditorOutcome outcome = session.hoverTarget(target);
        QVERIFY(!outcome.ok);
        QCOMPARE(outcome.code, EditorErrorCode::CommandFailed);
        QCOMPARE(session.gestureState(), GestureState::Idle);
        QVERIFY(!engine.hasPreview());

        const auto executed = engine.executed();
        QCOMPARE(executed.back(), CommandKind::CancelPreview);
        // The engine contract keeps snapshot, revision, and history intact on
        // the failed command; the cancel restores the pre-gesture profile.
    }

    void failedCommitRollsBackInsteadOfLeavingAPreview()
    {
        ScriptedEngine engine(profile());
        engine.failNext(CommandKind::CommitPreview);
        EditorSession session(engine, UserProfileStore{QStringLiteral("/unused")});

        QVERIFY(session.armDrag(instancePayload(QStringLiteral("bar"),
                                                QStringLiteral("clock-instance")))
                    .ok);
        QVERIFY(session.beginVisualDrag().ok);
        const DropTarget target{QStringLiteral("bar"), QStringLiteral("end"),
                                QStringLiteral("launcher-instance")};
        QVERIFY(session.hoverTarget(target).ok);

        const EditorOutcome outcome = session.drop();
        QVERIFY(!outcome.ok);
        QCOMPARE(outcome.code, EditorErrorCode::CommandFailed);
        QCOMPARE(session.gestureState(), GestureState::Idle);
        QVERIFY(!engine.hasPreview());
        QCOMPARE(engine.executed().back(), CommandKind::CancelPreview);
        QVERIFY(!session.isDirty());
    }

    void undoAndRedoAreGatedWhileAGestureIsOpen()
    {
        // Invariant 3: history controls are enabled only while idle.
        ScriptedEngine engine(profile());
        EditorSession session(engine, UserProfileStore{QStringLiteral("/unused")});

        QVERIFY(session.armDrag(instancePayload(QStringLiteral("bar"),
                                                QStringLiteral("clock-instance")))
                    .ok);
        QVERIFY(session.beginVisualDrag().ok);

        QVERIFY(!session.canUndo());
        QVERIFY(!session.canRedo());
        QCOMPARE(session.undo().code, EditorErrorCode::GestureRefused);
        QCOMPARE(session.redo().code, EditorErrorCode::GestureRefused);

        QVERIFY(session.cancelGesture().ok);
        QCOMPARE(session.canUndo(), engine.status().canUndo);
    }

    void applyGestureRollsBackCompletelyOnFailure()
    {
        ScriptedEngine engine(profile());
        engine.failNext(CommandKind::RemoveApplet);
        EditorSession session(engine, UserProfileStore{QStringLiteral("/unused")});

        const EditorOutcome outcome = session.applyGesture(
            removeIntent(QStringLiteral("bar"), QStringLiteral("clock-instance")),
            DropTarget{QStringLiteral("bar"), QStringLiteral("end"), {}});

        QVERIFY(!outcome.ok);
        QCOMPARE(outcome.code, EditorErrorCode::CommandFailed);
        QVERIFY(!engine.hasPreview());
        // BeginPreview executed, the remove failed, and the inline rollback
        // closed the bracket.
        QCOMPARE(engine.executed().size(), size_t{2});
        QCOMPARE(engine.executed().back(), CommandKind::CancelPreview);
        QVERIFY(!session.isDirty());
    }

    void applyGestureCommitsAtomically()
    {
        ScriptedEngine engine(profile());
        EditorSession session(engine, UserProfileStore{QStringLiteral("/unused")});

        QVERIFY(session.applyGesture(
                        removeIntent(QStringLiteral("bar"), QStringLiteral("clock-instance")),
                        DropTarget{QStringLiteral("bar"), QStringLiteral("end"), {}})
                    .ok);

        const auto executed = engine.executed();
        QCOMPARE(executed.size(), size_t{3});
        QCOMPARE(executed.at(0), CommandKind::BeginPreview);
        QCOMPARE(executed.at(1), CommandKind::RemoveApplet);
        QCOMPARE(executed.at(2), CommandKind::CommitPreview);
        QVERIFY(session.isDirty());
    }

    void productionCrossPanelZoneMoveChainsAndCreatesOneUndoStep()
    {
        const Profiles::LayoutProfile initial = profile();
        LayoutEditingRepository repository(initial, outputs(), manifests());
        QVERIFY(repository.isReady());
        CoordinatorEditingEngine engine(repository, manifests());
        QVERIFY(engine.holdsLease());
        EditorSession session(engine, UserProfileStore{QStringLiteral("/unused")});

        QVERIFY(session.armDrag(instancePayload(QStringLiteral("bar"),
                                                QStringLiteral("clock-instance"))).ok);
        QVERIFY(session.beginVisualDrag().ok);
        const DropTarget target{QStringLiteral("dock"), QStringLiteral("start"),
                                QStringLiteral("tasks-instance")};
        QVERIFY2(session.hoverTarget(target).ok,
                 qPrintable(session.lastOutcome().message));
        QVERIFY(session.drop().ok);
        QCOMPARE(repository.snapshot()->revision, quint64{4});

        const Profiles::PanelSpec *dock = panel(repository.snapshot()->profile,
                                                QStringLiteral("dock"));
        QVERIFY(dock != nullptr);
        const auto moved = std::find_if(dock->applets.cbegin(),
                                        dock->applets.cend(),
                                        [](const Profiles::AppletSpec &candidate) {
                                            return candidate.id == QLatin1String("clock-instance");
                                        });
        QVERIFY(moved != dock->applets.cend());
        QCOMPARE(moved->settings.value(QStringLiteral("zone")).toString(),
                 QStringLiteral("start"));

        QVERIFY(session.undo().ok);
        QCOMPARE(repository.snapshot()->profile.toJson(), initial.toJson());
        QVERIFY(!repository.status().canUndo);
        QVERIFY(repository.status().canRedo);
    }

    void rejectedReleaseCancelsEarlierAcceptedPreview()
    {
        ScriptedEngine engine(profile());
        EditorSession session(engine, UserProfileStore{QStringLiteral("/unused")});

        QVERIFY(session.armDrag(instancePayload(QStringLiteral("bar"),
                                                QStringLiteral("clock-instance"))).ok);
        QVERIFY(session.beginVisualDrag().ok);
        QVERIFY(session.hoverTarget(DropTarget{QStringLiteral("bar"),
                                               QStringLiteral("start"),
                                               QStringLiteral("launcher-instance")}).ok);
        engine.setEvaluationAccepted(false);
        QVERIFY(session.hoverTarget(DropTarget{QStringLiteral("dock"),
                                               QStringLiteral("center"),
                                               QStringLiteral("tasks-instance")}).ok);
        QVERIFY(session.drop().ok);
        QCOMPARE(engine.executed().back(), CommandKind::CancelPreview);
        QVERIFY(!session.isDirty());
        QVERIFY(!engine.hasPreview());
    }

    void productionCancelRestoresTheExactPreGestureProfile()
    {
        const Profiles::LayoutProfile initial = profile();
        LayoutEditingRepository repository(initial, outputs(), manifests());
        CoordinatorEditingEngine engine(repository, manifests());
        EditorSession session(engine, UserProfileStore{QStringLiteral("/unused")});

        QVERIFY(session.armDrag(instancePayload(QStringLiteral("bar"),
                                                QStringLiteral("clock-instance"))).ok);
        QVERIFY(session.beginVisualDrag().ok);
        QVERIFY2(session.hoverTarget(DropTarget{QStringLiteral("dock"),
                                               QStringLiteral("start"),
                                               QStringLiteral("tasks-instance")}).ok,
                 qPrintable(session.lastOutcome().message));
        QVERIFY(session.cancelGesture().ok);
        QCOMPARE(repository.snapshot()->profile.toJson(), initial.toJson());
        QCOMPARE(repository.status(), LayoutEditingStatus{});
        QVERIFY(!session.isDirty());
    }

    void returningFromAnInvalidHoverKeepsTheAppliedTargetDroppable()
    {
        LayoutEditingRepository repository(profile(), outputs(), manifests());
        CoordinatorEditingEngine engine(repository, manifests());
        EditorSession session(engine, UserProfileStore{QStringLiteral("/unused")});
        const DropTarget accepted{QStringLiteral("dock"), QStringLiteral("start"),
                                  QStringLiteral("tasks-instance")};

        QVERIFY(session.armDrag(instancePayload(QStringLiteral("bar"),
                                                QStringLiteral("clock-instance"))).ok);
        QVERIFY(session.beginVisualDrag().ok);
        QVERIFY(session.hoverTarget(accepted).ok);
        QVERIFY(session.hoverTarget(DropTarget{QString(), QStringLiteral("start"), {}}).ok);
        QVERIFY(session.acceptance().has_value());
        QVERIFY(!session.acceptance()->accepted);
        QVERIFY(session.hoverTarget(accepted).ok);
        QVERIFY(session.acceptance().has_value());
        QVERIFY(session.acceptance()->accepted);
        QVERIFY(session.drop().ok);
        QVERIFY(session.isDirty());

        const Profiles::PanelSpec *dock = panel(repository.snapshot()->profile,
                                                QStringLiteral("dock"));
        QVERIFY(dock != nullptr);
        QVERIFY(std::any_of(dock->applets.cbegin(), dock->applets.cend(),
                            [](const Profiles::AppletSpec &candidate) {
                                return candidate.id == QLatin1String("clock-instance");
                            }));
    }

    void applyGestureRejectsStructurallyInvalidIntents()
    {
        ScriptedEngine engine(profile());
        EditorSession session(engine, UserProfileStore{QStringLiteral("/unused")});

        const EditorOutcome outcome = session.applyGesture(
            duplicateIntent(QStringLiteral("bar"), QStringLiteral("clock-instance"), QString()),
            DropTarget{QStringLiteral("bar"), QStringLiteral("end"), {}});
        QVERIFY(!outcome.ok);
        QCOMPARE(outcome.code, EditorErrorCode::IntentInvalid);
        QVERIFY(engine.executed().empty());
    }

    void failedApplyKeepsTheSessionDirtyAndTyped()
    {
        // Persistence rollback: a failed user-profile write must not clear the
        // dirty flag, change the profile, or leak a partial file (architecture
        // D12; atomicity is proven by the persistence test). The store
        // directory is an existing regular file, so creating the directory
        // fails deterministically regardless of privileges.
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile directoryBlocker{directory.filePath(QStringLiteral("asdir"))};
        QVERIFY(directoryBlocker.open(QIODevice::WriteOnly));
        directoryBlocker.close();

        ScriptedEngine engine(profile());
        EditorSession session(engine, UserProfileStore{directoryBlocker.fileName()});

        QVERIFY(session.applyGesture(
                        removeIntent(QStringLiteral("bar"), QStringLiteral("clock-instance")),
                        DropTarget{QStringLiteral("bar"), QStringLiteral("end"), {}})
                    .ok);
        QVERIFY(session.isDirty());

        const EditorOutcome outcome = session.applyToUserProfile();
        QVERIFY(!outcome.ok);
        QCOMPARE(outcome.code, EditorErrorCode::ApplyFailed);
        QVERIFY(session.isDirty());
    }

    void applyRefusesAnOpenPreview()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ScriptedEngine engine(profile());
        EditorSession session(engine, UserProfileStore{directory.path()});

        QVERIFY(session.armDrag(instancePayload(QStringLiteral("bar"),
                                                QStringLiteral("clock-instance"))).ok);
        QVERIFY(session.beginVisualDrag().ok);
        const EditorOutcome outcome = session.applyToUserProfile();
        QVERIFY(!outcome.ok);
        QCOMPARE(outcome.code, EditorErrorCode::GestureRefused);
        QVERIFY(QDir(directory.path()).isEmpty());
        QVERIFY(session.cancelGesture().ok);
    }

    void revertRequiresHostRebuildWithoutPublishingFalseCleanState()
    {
        ScriptedEngine engine(profile());
        EditorSession session(engine, UserProfileStore{QStringLiteral("/unused")});

        QVERIFY(session.applyGesture(
                        removeIntent(QStringLiteral("bar"), QStringLiteral("clock-instance")),
                        DropTarget{QStringLiteral("bar"), QStringLiteral("end"), {}})
                    .ok);
        QVERIFY(session.isDirty());

        const quint64 revisionBefore = engine.revision();
        const EditorOutcome outcome = session.revert();
        QVERIFY(!outcome.ok);
        QCOMPARE(outcome.code, EditorErrorCode::RebuildRequired);
        QVERIFY(session.requiresRebuild());
        QVERIFY(session.isDirty());
        QCOMPARE(engine.revision(), revisionBefore);
        QCOMPARE(session.applyToUserProfile().code, EditorErrorCode::RebuildRequired);
        QCOMPARE(session.undo().code, EditorErrorCode::RebuildRequired);
    }

    void productionAdapterRetriesALostLeaseOnTheNextAction()
    {
        LayoutEditingRepository repository(profile(), outputs(), manifests());
        auto blocking = repository.tryAcquireCoordinator();
        QVERIFY(blocking);

        CoordinatorEditingEngine engine(repository, manifests());
        QVERIFY(!engine.holdsLease());
        QCOMPARE(engine.status(), LayoutEditingStatus{});

        blocking.reset();
        QVERIFY(engine.holdsLease());
        const EditingResult opened = engine.execute(BeginPreviewCommand{0});
        QVERIFY2(opened.succeeded(), qPrintable(opened.error.message));
        QVERIFY(engine.hasPreview());
        QVERIFY(engine.execute(CancelPreviewCommand{1}).succeeded());
    }

    void productionAdapterRejectsCrossThreadCallsWithoutMutation()
    {
        LayoutEditingRepository repository(profile(), outputs(), manifests());
        CoordinatorEditingEngine engine(repository, manifests());
        EditingResult crossThread;

        std::unique_ptr<QThread> caller{QThread::create([&] {
            crossThread = engine.execute(BeginPreviewCommand{0});
        })};
        caller->start();
        QVERIFY(caller->wait());

        QCOMPARE(crossThread.error.code,
                 QindaQt::ShellCustomization::EditingErrorCode::RepositoryNotReady);
        QVERIFY(crossThread.error.message.contains(QStringLiteral("owner thread")));
        QCOMPARE(repository.snapshot()->revision, quint64{0});
        QVERIFY(!repository.status().previewActive);
        QVERIFY(engine.holdsLease());
    }

    void outputGenerationChangeStalesTheSession()
    {
        // Invariant 6 / D16: the change closes any open gesture and marks the
        // session stale until the host rebuilds.
        ScriptedEngine engine(profile());
        EditorSession session(engine, UserProfileStore{QStringLiteral("/unused")});

        QVERIFY(session.armDrag(instancePayload(QStringLiteral("bar"),
                                                QStringLiteral("clock-instance")))
                    .ok);
        QVERIFY(session.beginVisualDrag().ok);

        QCOMPARE(session.notifyOutputGenerationChanged().code, EditorErrorCode::SessionStale);
        QVERIFY(session.isStale());
        QCOMPARE(session.gestureState(), GestureState::Idle);
        QVERIFY(!engine.hasPreview());
        QCOMPARE(engine.executed().back(), CommandKind::CancelPreview);

        QVERIFY(!session.armDrag(palettePayload(QStringLiteral("clock"))).ok);
        QCOMPARE(session.armDrag(palettePayload(QStringLiteral("clock"))).code,
                 EditorErrorCode::SessionStale);
    }

    void acceptanceIsDiscardedAfterTheRevisionMoves()
    {
        // Invariant 4: acceptance reserves nothing and must not survive a
        // revision change.
        ScriptedEngine engine(profile());
        EditorSession session(engine, UserProfileStore{QStringLiteral("/unused")});

        QVERIFY(session.armDrag(instancePayload(QStringLiteral("bar"),
                                                QStringLiteral("clock-instance")))
                    .ok);
        QVERIFY(session.beginVisualDrag().ok);
        const DropTarget target{QStringLiteral("bar"), QStringLiteral("start"),
                                QStringLiteral("launcher-instance")};
        QVERIFY(session.hoverTarget(target).ok);

        // The accepted hover executed and moved the revision; the highlight
        // computed before the execution must already be gone.
        QVERIFY(!session.acceptance().has_value());
    }
};

} // namespace

QTEST_MAIN(EditorSessionTest)
#include "tst_editor_session.moc"
