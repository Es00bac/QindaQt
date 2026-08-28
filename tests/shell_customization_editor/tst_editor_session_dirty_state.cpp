// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_test_fixtures.h"

#include "qindaqt/profiles/profile_loader.h"
#include "qindaqt/shell_customization/layout_editing_repository.h"
#include "qindaqt/shell_customization_editor/coordinator_engine_adapter.h"
#include "qindaqt/shell_customization_editor/editor_session.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace QindaQt;
using namespace QindaQt::ShellCustomizationEditor;
using namespace QindaQt::ShellCustomizationEditor::TestFixtures;

namespace {

class EditorSessionDirtyStateTest final : public QObject
{
    Q_OBJECT

private slots:
    void productionEditThenUndoRestoresConstructorBaselineAndCleanState()
    {
        const Profiles::LayoutProfile initial = profile();
        ShellCustomization::LayoutEditingRepository repository(
            initial, outputs(), manifests());
        QVERIFY(repository.isReady());
        CoordinatorEditingEngine engine(repository, manifests());
        QVERIFY(engine.holdsLease());
        EditorSession session(engine, UserProfileStore{QStringLiteral("/unused")});

        QCOMPARE(session.appliedProfileId(), initial.id);
        QVERIFY(!session.isDirty());
        QVERIFY(session.armDrag(instancePayload(QStringLiteral("bar"),
                                                QStringLiteral("clock-instance"))).ok);
        QVERIFY(session.beginVisualDrag().ok);
        const DropTarget target{QStringLiteral("dock"), QStringLiteral("start"),
                                QStringLiteral("tasks-instance")};
        QVERIFY2(session.hoverTarget(target).ok,
                 qPrintable(session.lastOutcome().message));
        QVERIFY(session.drop().ok);
        QCOMPARE(repository.snapshot()->revision, quint64{4});
        QVERIFY(repository.snapshot()->profile.toJson() != initial.toJson());
        QVERIFY(session.isDirty());

        QVERIFY(session.undo().ok);
        QCOMPARE(repository.snapshot()->profile.toJson(), initial.toJson());
        QVERIFY(!session.isDirty());
        QVERIFY(!repository.status().canUndo);
        QVERIFY(repository.status().canRedo);
    }

    void productionApplyThenUndoRedoTracksTheNewAppliedBaseline()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const Profiles::LayoutProfile initial = profile();
        ShellCustomization::LayoutEditingRepository repository(
            initial, outputs(), manifests());
        QVERIFY(repository.isReady());
        CoordinatorEditingEngine engine(repository, manifests());
        EditorSession session(engine, UserProfileStore{directory.path()});

        QVERIFY(session.applyGesture(
                        removeIntent(QStringLiteral("bar"),
                                     QStringLiteral("clock-instance")),
                        DropTarget{QStringLiteral("bar"), QStringLiteral("end"), {}})
                    .ok);
        const Profiles::LayoutProfile applied = repository.snapshot()->profile;
        QVERIFY(applied.toJson() != initial.toJson());
        QVERIFY(session.isDirty());

        const EditorOutcome apply = session.applyToUserProfile();
        QVERIFY2(apply.ok, qPrintable(apply.message));
        QVERIFY(!session.isDirty());
        QCOMPARE(session.appliedProfileId(), applied.id);
        const Profiles::LoadResult persisted = Profiles::ProfileLoader::fromFile(
            directory.filePath(UserProfileStore::fileNameForId(applied.id)));
        QVERIFY2(persisted.ok, qPrintable(persisted.error.diagnostic()));
        QCOMPARE(persisted.profile.toJson(), applied.toJson());

        QVERIFY(session.undo().ok);
        QCOMPARE(repository.snapshot()->profile.toJson(), initial.toJson());
        QVERIFY(session.isDirty());

        QVERIFY(session.redo().ok);
        QCOMPARE(repository.snapshot()->profile.toJson(), applied.toJson());
        QVERIFY(!session.isDirty());
    }
};

} // namespace

QTEST_MAIN(EditorSessionDirtyStateTest)
#include "tst_editor_session_dirty_state.moc"
