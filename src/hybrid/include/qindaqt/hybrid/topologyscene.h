// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/hybrid/topologycommand.h>
#include <qindaqt/hybrid/windowtopology.h>

#include <QString>

#include <memory>
#include <utility>

namespace QindaQt::Hybrid {

struct SceneStepResult final
{
    [[nodiscard]] static SceneStepResult ready() { return {true, {}}; }
    [[nodiscard]] static SceneStepResult failure(QString message)
    {
        return {false, std::move(message)};
    }

    bool succeeded = false;
    QString message;
};

// One instance represents one candidate transition. prepare() may stage scene
// state, commit() applies it, and rollback() must restore everything staged or
// applied after either failure. Implementations must not publish model state or
// retain snapshot references after the call returns.
class SceneTransaction
{
public:
    virtual ~SceneTransaction() = default;

    // AGENT-CONTRACT: The candidate has revision before.revision()+1 and is
    // globally valid. The repository still exposes before during both calls.
    [[nodiscard]] virtual SceneStepResult prepare(const WindowTopology &before,
                                                  const WindowTopology &candidate,
                                                  const TopologyCommand &command) = 0;
    [[nodiscard]] virtual SceneStepResult commit() = 0;
    virtual void rollback() noexcept = 0;
};

// The coordinator does not own the factory. It must outlive the coordinator and
// return a fresh, non-shared transaction for every execute() call.
class SceneTransactionFactory
{
public:
    virtual ~SceneTransactionFactory() = default;
    [[nodiscard]] virtual std::unique_ptr<SceneTransaction> create() = 0;
};

} // namespace QindaQt::Hybrid
