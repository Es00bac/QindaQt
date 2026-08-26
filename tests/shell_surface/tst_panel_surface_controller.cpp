// SPDX-License-Identifier: GPL-3.0-or-later
#include "surface_test_fixtures.h"

#include "qindaqt/shell_surface/panel_surface_controller.h"

#include <QHash>
#include <QSet>
#include <QtTest>

#include <limits>
#include <memory>
#include <utility>

using namespace QindaQt;

namespace {

struct FakeBackendState {
    QHash<int, QVector<ShellSurface::PanelSurfaceConfiguration>> liveSets;
    int nextGeneration = 0;
    int prepareAttempts = 0;
    int publishAttempts = 0;
    bool failPrepare = false;
    bool failPublish = false;
    QSet<int> dismissedGenerations;
};

class FakePublishedSurfaceSet final : public ShellSurface::PublishedSurfaceSet {
public:
    FakePublishedSurfaceSet(std::shared_ptr<FakeBackendState> state, int generation)
        : m_state(std::move(state))
        , m_generation(generation)
    {
    }

    ~FakePublishedSurfaceSet() override
    {
        m_state->liveSets.remove(m_generation);
    }

    bool isLive() const noexcept override
    {
        return m_state->liveSets.contains(m_generation) &&
            !m_state->dismissedGenerations.contains(m_generation);
    }

private:
    std::shared_ptr<FakeBackendState> m_state;
    int m_generation = 0;
};

class FakePreparedSurfaceSet final : public ShellSurface::PreparedSurfaceSet {
public:
    FakePreparedSurfaceSet(
        std::shared_ptr<FakeBackendState> state,
        QVector<ShellSurface::PanelSurfaceConfiguration> configurations)
        : m_state(std::move(state))
        , m_configurations(std::move(configurations))
    {
    }

    std::unique_ptr<ShellSurface::PublishedSurfaceSet> publish(QString *error) override
    {
        ++m_state->publishAttempts;
        if (m_state->failPublish) {
            if (error) {
                *error = QStringLiteral("injected publish failure");
            }
            return nullptr;
        }
        const int generation = ++m_state->nextGeneration;
        m_state->liveSets.insert(generation, m_configurations);
        return std::make_unique<FakePublishedSurfaceSet>(m_state, generation);
    }

private:
    std::shared_ptr<FakeBackendState> m_state;
    QVector<ShellSurface::PanelSurfaceConfiguration> m_configurations;
};

class FakePanelSurfaceBackend final : public ShellSurface::PanelSurfaceBackend {
public:
    FakePanelSurfaceBackend()
        : state(std::make_shared<FakeBackendState>())
    {
    }

    std::unique_ptr<ShellSurface::PreparedSurfaceSet> prepare(
        const QVector<ShellSurface::PanelSurfaceConfiguration> &configurations,
        QString *error) override
    {
        ++state->prepareAttempts;
        if (state->failPrepare) {
            if (error) {
                *error = QStringLiteral("injected prepare failure");
            }
            return nullptr;
        }
        return std::make_unique<FakePreparedSurfaceSet>(state, configurations);
    }

    QVector<ShellSurface::PanelSurfaceConfiguration> activeConfigurations() const
    {
        int newest = 0;
        for (auto it = state->liveSets.cbegin(); it != state->liveSets.cend(); ++it) {
            newest = qMax(newest, it.key());
        }
        return state->liveSets.value(newest);
    }

    std::shared_ptr<FakeBackendState> state;
};

} // namespace

class PanelSurfaceControllerTests final : public QObject {
    Q_OBJECT

private slots:
    void publishesAnInitialSetAndSkipsAnIdenticalPlan();
    void republishesAnIdenticalPlanAfterCompositorDismissal();
    void replacesTheWholePublishedSetAfterSuccessfulStaging();
    void preservesStateWhenPlanningFails();
    void preservesStateWhenBackendPreparationFails();
    void preservesStateWhenBackendPublicationFails();
    void publishesAnEmptyReplacementToClearPanels();
    void rejectsRevisionExhaustionBeforeBackendMutation();
    void destroysPublishedSurfacesWithTheController();
};

void PanelSurfaceControllerTests::publishesAnInitialSetAndSkipsAnIdenticalPlan()
{
    using namespace ShellSurface::TestFixtures;
    FakePanelSurfaceBackend backend;
    ShellSurface::PanelSurfaceController controller(backend, 41);
    const auto layout = solve({panel(QStringLiteral("top"))});

    const auto first = controller.reconcile(layout);
    QVERIFY2(first.ok(), qPrintable(first.message));
    QVERIFY(first.changed);
    QCOMPARE(first.revision, quint64(42));
    QCOMPARE(backend.state->prepareAttempts, 1);
    QCOMPARE(backend.state->publishAttempts, 1);
    QCOMPARE(backend.activeConfigurations().size(), 1);
    QVERIFY(controller.hasPublishedSet());

    const auto repeated = controller.reconcile(layout);
    QVERIFY(repeated.ok());
    QVERIFY(!repeated.changed);
    QCOMPARE(repeated.revision, quint64(42));
    QCOMPARE(backend.state->prepareAttempts, 1);
    QCOMPARE(backend.state->publishAttempts, 1);
}

void PanelSurfaceControllerTests::republishesAnIdenticalPlanAfterCompositorDismissal()
{
    using namespace ShellSurface::TestFixtures;
    FakePanelSurfaceBackend backend;
    ShellSurface::PanelSurfaceController controller(backend);
    const auto layout = solve({panel(QStringLiteral("top"))});
    QVERIFY(controller.reconcile(layout).ok());

    backend.state->dismissedGenerations.insert(backend.state->nextGeneration);
    const auto recovered = controller.reconcile(layout);

    QVERIFY2(recovered.ok(), qPrintable(recovered.message));
    QVERIFY(recovered.changed);
    QCOMPARE(recovered.revision, quint64(2));
    QCOMPARE(backend.state->prepareAttempts, 2);
    QCOMPARE(backend.state->publishAttempts, 2);
    QCOMPARE(backend.state->liveSets.size(), 1);
    QVERIFY(controller.hasPublishedSet());
}

void PanelSurfaceControllerTests::replacesTheWholePublishedSetAfterSuccessfulStaging()
{
    using namespace ShellSurface::TestFixtures;
    FakePanelSurfaceBackend backend;
    ShellSurface::PanelSurfaceController controller(backend);
    QVERIFY(controller.reconcile(solve({panel(QStringLiteral("top"))})).ok());
    QCOMPARE(backend.state->liveSets.size(), 1);

    auto changedPanel = panel(QStringLiteral("top"));
    changedPanel.thickness = 48;
    const auto changed = controller.reconcile(solve({changedPanel}));
    QVERIFY2(changed.ok(), qPrintable(changed.message));
    QVERIFY(changed.changed);
    QCOMPARE(changed.revision, quint64(2));
    QCOMPARE(backend.state->liveSets.size(), 1);
    QCOMPARE(backend.activeConfigurations().constFirst().geometry.height(), 48);
    QCOMPARE(controller.currentPlan().surfaces.constFirst().geometry.height(), 48);
}

void PanelSurfaceControllerTests::preservesStateWhenPlanningFails()
{
    using namespace ShellSurface::TestFixtures;
    FakePanelSurfaceBackend backend;
    ShellSurface::PanelSurfaceController controller(backend);
    QVERIFY(controller.reconcile(solve({panel(QStringLiteral("top"))})).ok());
    const auto retainedPlan = controller.currentPlan();
    const auto retainedConfigurations = backend.activeConfigurations();

    auto rejected = solve({panel(QStringLiteral("other"))});
    rejected.error.code = ShellLayout::PanelLayoutErrorCode::InvalidPanel;
    rejected.error.message = QStringLiteral("injected layout rejection");
    const auto result = controller.reconcile(rejected);
    QVERIFY(!result.ok());
    QCOMPARE(result.code, ShellSurface::PanelSurfaceControllerErrorCode::PlanningFailed);
    QCOMPARE(result.revision, quint64(1));
    QCOMPARE(controller.currentPlan(), retainedPlan);
    QCOMPARE(backend.activeConfigurations(), retainedConfigurations);
    QCOMPARE(backend.state->prepareAttempts, 1);
}

void PanelSurfaceControllerTests::preservesStateWhenBackendPreparationFails()
{
    using namespace ShellSurface::TestFixtures;
    FakePanelSurfaceBackend backend;
    ShellSurface::PanelSurfaceController controller(backend);
    QVERIFY(controller.reconcile(solve({panel(QStringLiteral("top"))})).ok());
    const auto retainedPlan = controller.currentPlan();
    const auto retainedConfigurations = backend.activeConfigurations();
    backend.state->failPrepare = true;

    auto changed = panel(QStringLiteral("top"));
    changed.thickness = 50;
    const auto result = controller.reconcile(solve({changed}));
    QVERIFY(!result.ok());
    QCOMPARE(result.code, ShellSurface::PanelSurfaceControllerErrorCode::BackendPrepareFailed);
    QCOMPARE(result.message, QStringLiteral("injected prepare failure"));
    QCOMPARE(result.revision, quint64(1));
    QCOMPARE(controller.currentPlan(), retainedPlan);
    QCOMPARE(backend.activeConfigurations(), retainedConfigurations);
    QCOMPARE(backend.state->liveSets.size(), 1);
}

void PanelSurfaceControllerTests::preservesStateWhenBackendPublicationFails()
{
    using namespace ShellSurface::TestFixtures;
    FakePanelSurfaceBackend backend;
    ShellSurface::PanelSurfaceController controller(backend);
    QVERIFY(controller.reconcile(solve({panel(QStringLiteral("top"))})).ok());
    const auto retainedPlan = controller.currentPlan();
    const auto retainedConfigurations = backend.activeConfigurations();
    backend.state->failPublish = true;

    auto changed = panel(QStringLiteral("top"));
    changed.thickness = 52;
    const auto result = controller.reconcile(solve({changed}));
    QVERIFY(!result.ok());
    QCOMPARE(result.code, ShellSurface::PanelSurfaceControllerErrorCode::BackendPublishFailed);
    QCOMPARE(result.message, QStringLiteral("injected publish failure"));
    QCOMPARE(result.revision, quint64(1));
    QCOMPARE(controller.currentPlan(), retainedPlan);
    QCOMPARE(backend.activeConfigurations(), retainedConfigurations);
    QCOMPARE(backend.state->liveSets.size(), 1);
}

void PanelSurfaceControllerTests::publishesAnEmptyReplacementToClearPanels()
{
    using namespace ShellSurface::TestFixtures;
    FakePanelSurfaceBackend backend;
    ShellSurface::PanelSurfaceController controller(backend);
    QVERIFY(controller.reconcile(solve({panel(QStringLiteral("top"))})).ok());

    const auto cleared = controller.reconcile(solve({}));
    QVERIFY2(cleared.ok(), qPrintable(cleared.message));
    QVERIFY(cleared.changed);
    QCOMPARE(cleared.revision, quint64(2));
    QVERIFY(backend.activeConfigurations().isEmpty());
    QVERIFY(controller.currentPlan().surfaces.isEmpty());
    QVERIFY(controller.hasPublishedSet());
}

void PanelSurfaceControllerTests::rejectsRevisionExhaustionBeforeBackendMutation()
{
    using namespace ShellSurface::TestFixtures;
    FakePanelSurfaceBackend backend;
    ShellSurface::PanelSurfaceController controller(backend,
                                                    std::numeric_limits<quint64>::max());
    const auto result = controller.reconcile(solve({panel(QStringLiteral("top"))}));
    QVERIFY(!result.ok());
    QCOMPARE(result.code, ShellSurface::PanelSurfaceControllerErrorCode::RevisionExhausted);
    QCOMPARE(result.revision, std::numeric_limits<quint64>::max());
    QCOMPARE(backend.state->prepareAttempts, 0);
    QVERIFY(!controller.hasPublishedSet());
}

void PanelSurfaceControllerTests::destroysPublishedSurfacesWithTheController()
{
    using namespace ShellSurface::TestFixtures;
    FakePanelSurfaceBackend backend;
    {
        ShellSurface::PanelSurfaceController controller(backend);
        QVERIFY(controller.reconcile(solve({panel(QStringLiteral("top"))})).ok());
        QCOMPARE(backend.state->liveSets.size(), 1);
    }
    QVERIFY(backend.state->liveSets.isEmpty());
}

QTEST_GUILESS_MAIN(PanelSurfaceControllerTests)
#include "tst_panel_surface_controller.moc"
