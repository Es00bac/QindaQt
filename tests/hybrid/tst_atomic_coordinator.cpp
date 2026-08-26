// SPDX-License-Identifier: GPL-3.0-or-later
#include "testfixtures.h"

#include <QtTest>

#include <functional>
#include <limits>

using namespace QindaQt::Hybrid;
using namespace QindaQt::Hybrid::Test;

namespace {

enum class SceneMode {
    Ready,
    FailPrepare,
    FailCommit,
    Unavailable,
};

struct SceneRecord final
{
    int created = 0;
    int prepared = 0;
    int committed = 0;
    int rolledBack = 0;
    quint64 beforeRevision = 0;
    quint64 candidateRevision = 0;
    quint64 repositoryRevisionDuringPrepare = 0;
    quint64 repositoryRevisionDuringCommit = 0;
    TopologyCommandKind kind = TopologyCommandKind::ReleaseContainer;
    bool candidateValid = false;
    bool beforeContainsProbe = false;
    bool candidateContainsProbe = false;
};

class RecordingTransaction final : public SceneTransaction
{
public:
    RecordingTransaction(SceneMode mode,
                         SceneRecord &record,
                         const TopologyRepository &repository,
                         std::function<void()> prepareCallback)
        : m_mode(mode)
        , m_record(record)
        , m_repository(repository)
        , m_prepareCallback(std::move(prepareCallback))
    {
    }

    SceneStepResult prepare(const WindowTopology &before,
                            const WindowTopology &candidate,
                            const TopologyCommand &command) override
    {
        ++m_record.prepared;
        m_record.beforeRevision = before.revision();
        m_record.candidateRevision = candidate.revision();
        m_record.repositoryRevisionDuringPrepare = m_repository.topology().revision();
        m_record.kind = commandKind(command);
        m_record.candidateValid = candidate.validate().valid;
        const QString probe = QStringLiteral("window-probe");
        m_record.beforeContainsProbe = before.isIndependent(probe) || before.ownerOf(probe);
        m_record.candidateContainsProbe = candidate.isIndependent(probe)
            || candidate.ownerOf(probe);
        if (m_prepareCallback) {
            m_prepareCallback();
        }
        return m_mode == SceneMode::FailPrepare
            ? SceneStepResult::failure(QStringLiteral("prepare sentinel"))
            : SceneStepResult::ready();
    }

    SceneStepResult commit() override
    {
        ++m_record.committed;
        m_record.repositoryRevisionDuringCommit = m_repository.topology().revision();
        return m_mode == SceneMode::FailCommit
            ? SceneStepResult::failure(QStringLiteral("commit sentinel"))
            : SceneStepResult::ready();
    }

    void rollback() noexcept override { ++m_record.rolledBack; }

private:
    SceneMode m_mode;
    SceneRecord &m_record;
    const TopologyRepository &m_repository;
    std::function<void()> m_prepareCallback;
};

class RecordingFactory final : public SceneTransactionFactory
{
public:
    RecordingFactory(SceneMode mode,
                     SceneRecord &record,
                     const TopologyRepository &repository)
        : m_mode(mode)
        , m_record(record)
        , m_repository(repository)
    {
    }

    std::unique_ptr<SceneTransaction> create() override
    {
        ++m_record.created;
        if (m_mode == SceneMode::Unavailable) {
            return {};
        }
        return std::make_unique<RecordingTransaction>(m_mode,
                                                      m_record,
                                                      m_repository,
                                                      m_prepareCallback);
    }

    void setPrepareCallback(std::function<void()> callback)
    {
        m_prepareCallback = std::move(callback);
    }

private:
    SceneMode m_mode;
    SceneRecord &m_record;
    const TopologyRepository &m_repository;
    std::function<void()> m_prepareCallback;
};

} // namespace

Q_DECLARE_METATYPE(SceneMode)
Q_DECLARE_METATYPE(TopologyCommandError)

class AtomicCoordinatorTest final : public QObject
{
    Q_OBJECT

private slots:
    void lifecycleCommandsUseAtomicSceneCandidate();
    void rejectsReentrantExecutionWithoutDisturbingOuterCommand();
    void publishesOnlyAfterSceneCommit();
    void rollsBackPrepareAndCommitFailures_data();
    void rollsBackPrepareAndCommitFailures();
    void rejectsBeforeCreatingSceneTransaction();
    void reportsUnavailableSceneAndRevisionExhaustion();
};

void AtomicCoordinatorTest::lifecycleCommandsUseAtomicSceneCandidate()
{
    TopologyRepository repository;
    SceneRecord record;
    RecordingFactory scene(SceneMode::Ready, record, repository);
    TopologyCoordinator coordinator(repository, scene);

    const auto added = coordinator.execute(
        AddIndependentWindow{QStringLiteral("window-probe")});
    QVERIFY2(added.committed(), qPrintable(added.message));
    QCOMPARE(record.kind, TopologyCommandKind::AddIndependentWindow);
    QVERIFY(!record.beforeContainsProbe);
    QVERIFY(record.candidateContainsProbe);
    QCOMPARE(record.repositoryRevisionDuringPrepare, quint64{0});
    QCOMPARE(record.repositoryRevisionDuringCommit, quint64{0});
    QVERIFY(repository.topology().isIndependent(QStringLiteral("window-probe")));

    record = {};
    const auto forgotten = coordinator.execute(
        ForgetWindow{QStringLiteral("window-probe")});
    QVERIFY2(forgotten.committed(), qPrintable(forgotten.message));
    QCOMPARE(record.kind, TopologyCommandKind::ForgetWindow);
    QVERIFY(record.beforeContainsProbe);
    QVERIFY(!record.candidateContainsProbe);
    QCOMPARE(record.repositoryRevisionDuringPrepare, quint64{1});
    QCOMPARE(record.repositoryRevisionDuringCommit, quint64{1});
    QVERIFY(!repository.topology().isIndependent(QStringLiteral("window-probe")));
    QVERIFY(!repository.topology().ownerOf(QStringLiteral("window-probe")));
    QCOMPARE(repository.topology().revision(), quint64{2});
}

void AtomicCoordinatorTest::rejectsReentrantExecutionWithoutDisturbingOuterCommand()
{
    TopologyRepository repository;
    SceneRecord record;
    RecordingFactory scene(SceneMode::Ready, record, repository);
    TopologyCoordinator coordinator(repository, scene);
    TopologyCommandResult nested;
    scene.setPrepareCallback([&] {
        nested = coordinator.execute(
            AddIndependentWindow{QStringLiteral("window-nested")});
    });

    const auto outer = coordinator.execute(
        AddIndependentWindow{QStringLiteral("window-probe")});
    QVERIFY(outer.committed());
    QCOMPARE(nested.error, TopologyCommandError::ReentrantExecution);
    QCOMPARE(nested.revision, quint64{0});
    QVERIFY(repository.topology().isIndependent(QStringLiteral("window-probe")));
    QVERIFY(!repository.topology().isIndependent(QStringLiteral("window-nested")));
    QCOMPARE(repository.topology().revision(), quint64{1});
}

void AtomicCoordinatorTest::publishesOnlyAfterSceneCommit()
{
    TopologyRepository repository(topology({QStringLiteral("window-a"),
                                            QStringLiteral("window-b")},
                                           {},
                                           40));
    SceneRecord record;
    RecordingFactory scene(SceneMode::Ready, record, repository);
    TopologyCoordinator coordinator(repository, scene);

    const auto result = coordinator.execute(
        dockCommand(QStringLiteral("container"),
                    QStringLiteral("ab"),
                    QStringLiteral("window-a"),
                    QStringLiteral("window-b")));
    QVERIFY2(result.committed(), qPrintable(result.message));
    QCOMPARE(result.previousRevision, quint64{40});
    QCOMPARE(result.revision, quint64{41});
    QCOMPARE(record.created, 1);
    QCOMPARE(record.prepared, 1);
    QCOMPARE(record.committed, 1);
    QCOMPARE(record.rolledBack, 0);
    QCOMPARE(record.beforeRevision, quint64{40});
    QCOMPARE(record.candidateRevision, quint64{41});
    QCOMPARE(record.repositoryRevisionDuringPrepare, quint64{40});
    QCOMPARE(record.repositoryRevisionDuringCommit, quint64{40});
    QCOMPARE(record.kind, TopologyCommandKind::DockIndependentWindows);
    QVERIFY(record.candidateValid);
    QCOMPARE(repository.topology().revision(), quint64{41});
    QVERIFY(repository.topology().container(QStringLiteral("container")));
}

void AtomicCoordinatorTest::rollsBackPrepareAndCommitFailures_data()
{
    QTest::addColumn<SceneMode>("mode");
    QTest::addColumn<TopologyCommandError>("expectedError");
    QTest::addColumn<int>("expectedCommitCalls");

    QTest::newRow("prepare") << SceneMode::FailPrepare
                             << TopologyCommandError::ScenePrepareFailed << 0;
    QTest::newRow("commit") << SceneMode::FailCommit
                            << TopologyCommandError::SceneCommitFailed << 1;
}

void AtomicCoordinatorTest::rollsBackPrepareAndCommitFailures()
{
    QFETCH(SceneMode, mode);
    QFETCH(TopologyCommandError, expectedError);
    QFETCH(int, expectedCommitCalls);

    TopologyRepository repository(topology({QStringLiteral("window-a"),
                                            QStringLiteral("window-b")},
                                           {},
                                           9));
    SceneRecord record;
    RecordingFactory scene(mode, record, repository);
    TopologyCoordinator coordinator(repository, scene);

    const auto result = coordinator.execute(
        dockCommand(QStringLiteral("container"),
                    QStringLiteral("ab"),
                    QStringLiteral("window-a"),
                    QStringLiteral("window-b")));
    QVERIFY(!result.committed());
    QCOMPARE(result.error, expectedError);
    QCOMPARE(result.revision, quint64{9});
    QCOMPARE(record.prepared, 1);
    QCOMPARE(record.committed, expectedCommitCalls);
    QCOMPARE(record.rolledBack, 1);
    QCOMPARE(repository.topology().revision(), quint64{9});
    QVERIFY(repository.topology().containerIds().isEmpty());
    QCOMPARE(repository.topology().independentWindowIds(),
             QStringList({QStringLiteral("window-a"), QStringLiteral("window-b")}));
}

void AtomicCoordinatorTest::rejectsBeforeCreatingSceneTransaction()
{
    TopologyRepository repository(topology({QStringLiteral("window-a"),
                                            QStringLiteral("window-b")},
                                           {},
                                           5));
    SceneRecord record;
    RecordingFactory scene(SceneMode::Ready, record, repository);
    TopologyCoordinator coordinator(repository, scene);
    auto command = dockCommand(QStringLiteral("container"),
                               QStringLiteral("ab"),
                               QStringLiteral("window-a"),
                               QStringLiteral("window-a"));

    const auto result = coordinator.execute(command);
    QVERIFY(!result.committed());
    QCOMPARE(result.error, TopologyCommandError::InvalidCommand);
    QCOMPARE(record.created, 0);
    QCOMPARE(repository.topology().revision(), quint64{5});
    QVERIFY(repository.topology().containerIds().isEmpty());
}

void AtomicCoordinatorTest::reportsUnavailableSceneAndRevisionExhaustion()
{
    TopologyRepository repository(topology({QStringLiteral("window-a"),
                                            QStringLiteral("window-b")},
                                           {},
                                           3));
    SceneRecord record;
    RecordingFactory unavailable(SceneMode::Unavailable, record, repository);
    TopologyCoordinator coordinator(repository, unavailable);
    const auto command = dockCommand(QStringLiteral("container"),
                                     QStringLiteral("ab"),
                                     QStringLiteral("window-a"),
                                     QStringLiteral("window-b"));

    const auto unavailableResult = coordinator.execute(command);
    QCOMPARE(unavailableResult.error, TopologyCommandError::SceneUnavailable);
    QCOMPARE(repository.topology().revision(), quint64{3});
    QCOMPARE(record.created, 1);

    TopologyRepository exhaustedRepository(
        topology({QStringLiteral("window-a"), QStringLiteral("window-b")},
                 {},
                 std::numeric_limits<quint64>::max()));
    SceneRecord exhaustedRecord;
    RecordingFactory ready(SceneMode::Ready, exhaustedRecord, exhaustedRepository);
    TopologyCoordinator exhaustedCoordinator(exhaustedRepository, ready);
    const auto exhausted = exhaustedCoordinator.execute(command);
    QCOMPARE(exhausted.error, TopologyCommandError::RevisionExhausted);
    QCOMPARE(exhaustedRecord.created, 0);
}

QTEST_APPLESS_MAIN(AtomicCoordinatorTest)

#include "tst_atomic_coordinator.moc"
