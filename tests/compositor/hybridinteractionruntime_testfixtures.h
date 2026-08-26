// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridinteractionruntime.h"

#include <memory>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration::Test {

class RecordingSceneFactory;

class RecordingSceneTransaction final : public Hybrid::SceneTransaction
{
public:
    RecordingSceneTransaction(RecordingSceneFactory &owner, bool commitSucceeds)
        : m_owner(owner)
        , m_commitSucceeds(commitSucceeds)
    {
    }

    Hybrid::SceneStepResult prepare(const Hybrid::WindowTopology &before,
                                    const Hybrid::WindowTopology &candidate,
                                    const Hybrid::TopologyCommand &command) override;
    Hybrid::SceneStepResult commit() override;
    void rollback() noexcept override;

private:
    RecordingSceneFactory &m_owner;
    bool m_commitSucceeds = true;
};

class RecordingSceneFactory final : public Hybrid::SceneTransactionFactory
{
public:
    explicit RecordingSceneFactory(QVector<bool> commitOutcomes = {})
        : outcomes(std::move(commitOutcomes))
    {
    }

    std::unique_ptr<Hybrid::SceneTransaction> create() override
    {
        const bool succeeds = nextOutcome < outcomes.size() ? outcomes[nextOutcome] : true;
        ++nextOutcome;
        ++created;
        return std::make_unique<RecordingSceneTransaction>(*this, succeeds);
    }

    QVector<bool> outcomes;
    QVector<Hybrid::TopologyCommandKind> kinds;
    QVector<quint64> beforeRevisions;
    QVector<quint64> candidateRevisions;
    qsizetype nextOutcome = 0;
    int created = 0;
    int prepared = 0;
    int committed = 0;
    int rolledBack = 0;
};

inline Hybrid::SceneStepResult RecordingSceneTransaction::prepare(
    const Hybrid::WindowTopology &before,
    const Hybrid::WindowTopology &candidate,
    const Hybrid::TopologyCommand &command)
{
    ++m_owner.prepared;
    m_owner.kinds.append(Hybrid::commandKind(command));
    m_owner.beforeRevisions.append(before.revision());
    m_owner.candidateRevisions.append(candidate.revision());
    return Hybrid::SceneStepResult::ready();
}

inline Hybrid::SceneStepResult RecordingSceneTransaction::commit()
{
    ++m_owner.committed;
    return m_commitSucceeds
        ? Hybrid::SceneStepResult::ready()
        : Hybrid::SceneStepResult::failure(QStringLiteral("commit sentinel"));
}

inline void RecordingSceneTransaction::rollback() noexcept
{
    ++m_owner.rolledBack;
}

inline HybridInput::InteractionIntent memberCommit(
    QString sourceWindow,
    QString sourceContainer,
    HybridInput::HitKind sourceKind,
    QString targetWindow,
    QString targetContainer,
    HybridInput::DockZone zone,
    QString sourcePage = {})
{
    HybridInput::InteractionIntent intent{
        .kind = HybridInput::InteractionKind::MemberDock,
        .phase = HybridInput::IntentPhase::Commit,
        .source = {sourceKind, std::move(sourceContainer), std::move(sourceWindow), {}},
        .target = {std::move(targetContainer), std::move(targetWindow), zone},
        .position = {},
        .delta = {},
    };
    intent.source.pageId = std::move(sourcePage);
    return intent;
}

inline HybridRuntimeResult dock(HybridInteractionRuntime &runtime,
                                const QString &source,
                                const QString &target,
                                HybridInput::DockZone zone)
{
    return runtime.handleIntent(memberCommit(source,
                                             {},
                                             HybridInput::HitKind::MemberTitle,
                                             target,
                                             {},
                                             zone));
}

} // namespace QindaQt::Compositor::KWinIntegration::Test
