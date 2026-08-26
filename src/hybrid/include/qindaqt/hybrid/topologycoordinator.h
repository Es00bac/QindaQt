// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/hybrid/topologycommand.h>
#include <qindaqt/hybrid/topologyscene.h>
#include <qindaqt/hybrid/windowtopology.h>

#include <QString>
#include <QtTypes>

namespace QindaQt::Hybrid {

enum class TopologyCommandError {
    None,
    InvalidCommand,
    InvalidCandidate,
    RevisionExhausted,
    ReentrantExecution,
    SceneUnavailable,
    ScenePrepareFailed,
    SceneCommitFailed,
};

struct TopologyCommandResult final
{
    [[nodiscard]] bool committed() const noexcept
    {
        return error == TopologyCommandError::None;
    }

    TopologyCommandKind kind = TopologyCommandKind::AddIndependentWindow;
    TopologyCommandError error = TopologyCommandError::InvalidCommand;
    quint64 previousRevision = 0;
    quint64 revision = 0;
    QString message;
};

// The coordinator borrows repository and sceneFactory for its full lifetime.
// execute() is synchronous and non-reentrant; callers must serialize it on the
// compositor thread. Failed commands never change the repository revision.
class TopologyCoordinator final
{
public:
    TopologyCoordinator(TopologyRepository &repository,
                        SceneTransactionFactory &sceneFactory) noexcept;

    [[nodiscard]] TopologyCommandResult execute(const TopologyCommand &command);

private:
    TopologyRepository &m_repository;
    SceneTransactionFactory &m_sceneFactory;
    bool m_executing = false;
};

} // namespace QindaQt::Hybrid
