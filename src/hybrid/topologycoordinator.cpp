// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/hybrid/topologycoordinator.h>

#include "topologymutation_p.h"

#include <limits>

namespace QindaQt::Hybrid {
namespace {

TopologyCommandResult failure(TopologyCommandKind kind,
                              TopologyCommandError error,
                              quint64 revision,
                              QString message)
{
    return {kind, error, revision, revision, std::move(message)};
}

QString sceneMessage(QString message, QLatin1StringView fallback)
{
    return message.isEmpty() ? fallback.toString() : std::move(message);
}

class ExecutionGuard final
{
public:
    explicit ExecutionGuard(bool &flag)
        : m_flag(flag)
    {
        m_flag = true;
    }
    ~ExecutionGuard() { m_flag = false; }

private:
    bool &m_flag;
};

} // namespace

TopologyCoordinator::TopologyCoordinator(TopologyRepository &repository,
                                         SceneTransactionFactory &sceneFactory) noexcept
    : m_repository(repository)
    , m_sceneFactory(sceneFactory)
{
}

TopologyCommandResult TopologyCoordinator::execute(const TopologyCommand &command)
{
    const auto kind = commandKind(command);
    const quint64 beforeRevision = m_repository.topology().revision();
    if (m_executing) {
        return failure(kind,
                       TopologyCommandError::ReentrantExecution,
                       beforeRevision,
                       QStringLiteral("topology command execution is not reentrant"));
    }
    ExecutionGuard guard(m_executing);

    if (beforeRevision == std::numeric_limits<quint64>::max()) {
        return failure(kind,
                       TopologyCommandError::RevisionExhausted,
                       beforeRevision,
                       QStringLiteral("topology revision is exhausted"));
    }

    WindowTopology candidate = m_repository.topology();
    QString mutationError;
    if (!TopologyMutation::apply(candidate, command, &mutationError)) {
        return failure(kind,
                       TopologyCommandError::InvalidCommand,
                       beforeRevision,
                       std::move(mutationError));
    }

    // AGENT-GUARD: Normalize and validate the private copy before touching the
    // scene. Publishing a singleton would violate the shell's one-frame cache
    // contract and make automatic unwrap observable as a second revision.
    TopologyMutation::normalize(candidate);
    candidate.m_revision = beforeRevision + 1;
    const auto validation = candidate.validate();
    if (!validation.valid) {
        return failure(kind,
                       TopologyCommandError::InvalidCandidate,
                       beforeRevision,
                       validation.message);
    }

    auto scene = m_sceneFactory.create();
    if (!scene) {
        return failure(kind,
                       TopologyCommandError::SceneUnavailable,
                       beforeRevision,
                       QStringLiteral("scene transaction factory returned no transaction"));
    }

    const auto prepared = scene->prepare(m_repository.topology(), candidate, command);
    if (!prepared.succeeded) {
        scene->rollback();
        return failure(kind,
                       TopologyCommandError::ScenePrepareFailed,
                       beforeRevision,
                       sceneMessage(prepared.message,
                                    QLatin1StringView("scene preparation failed")));
    }
    const auto committed = scene->commit();
    if (!committed.succeeded) {
        scene->rollback();
        return failure(kind,
                       TopologyCommandError::SceneCommitFailed,
                       beforeRevision,
                       sceneMessage(committed.message,
                                    QLatin1StringView("scene commit failed")));
    }

    // AGENT-CONTRACT: Scene commit precedes the repository's no-fail swap.
    // Scene adapters may query the repository during commit and must see the
    // old revision until every platform mutation has succeeded.
    m_repository.publish(std::move(candidate));
    return {kind,
            TopologyCommandError::None,
            beforeRevision,
            beforeRevision + 1,
            {}};
}

} // namespace QindaQt::Hybrid
