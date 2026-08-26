// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/compositor/containercontrolbridge.h"

#include "operationexecutor.h"
#include "qindaqt/compositor/sceneadapter.h"

#include <QScopedValueRollback>
#include <QThread>

#include <limits>
#include <utility>

namespace QindaQt::Compositor {
namespace {

bool assignError(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

} // namespace

ContainerControlBridge::ContainerControlBridge(SceneAdapter &adapter, QObject *parent)
    : QObject(parent)
    , m_adapter(adapter)
{
}

void ContainerControlBridge::assertThread() const
{
    Q_ASSERT_X(thread() == QThread::currentThread(),
               "ContainerControlBridge",
               "container control must execute on the owning compositor thread");
}

bool ContainerControlBridge::registerContainer(Core::WindowContainer container, QString *error)
{
    assertThread();
    if (m_applying) {
        return assignError(error, QStringLiteral("cannot register during a transaction"));
    }
    const auto validation = container.validate();
    if (!validation.valid) {
        return assignError(error, validation.message);
    }
    const auto containerId = container.id();
    if (m_containers.contains(containerId)) {
        return assignError(error, QStringLiteral("container '%1' is already registered").arg(containerId));
    }
    m_containers.insert(containerId, ContainerState{std::move(container), 0});
    return true;
}

bool ContainerControlBridge::unregisterContainer(const QString &containerId, QString *error)
{
    assertThread();
    if (m_applying) {
        return assignError(error, QStringLiteral("cannot unregister during a transaction"));
    }
    if (!m_containers.remove(containerId)) {
        return assignError(error, QStringLiteral("unknown container '%1'").arg(containerId));
    }
    return true;
}

bool ContainerControlBridge::contains(const QString &containerId) const
{
    assertThread();
    return m_containers.contains(containerId);
}

std::optional<quint64> ContainerControlBridge::revision(const QString &containerId) const
{
    assertThread();
    const auto iterator = m_containers.constFind(containerId);
    return iterator == m_containers.cend()
        ? std::nullopt
        : std::optional<quint64>(iterator->revision);
}

std::optional<QJsonObject> ContainerControlBridge::snapshot(const QString &containerId) const
{
    assertThread();
    const auto iterator = m_containers.constFind(containerId);
    return iterator == m_containers.cend()
        ? std::nullopt
        : std::optional<QJsonObject>(iterator->container.toJson());
}

ControlReply ContainerControlBridge::submitStagedSplit(Core::WindowContainer staging,
                                                        const ControlRequest &request)
{
    assertThread();
    const bool isSplit = request.operations.size() == 1
        && request.operations.constFirst().value(QStringLiteral("type"))
               == QStringLiteral("split-window");
    if (!staging.singleWindowId() || request.containerId != staging.id()
        || request.expectedRevision != 0 || !isSplit) {
        return reject(request, ReplyStatus::Rejected,
                      QStringLiteral("invalid-staging-request"),
                      QStringLiteral("staging requires one matching singleton and split operation"));
    }

    QString error;
    if (!registerContainer(std::move(staging), &error)) {
        return reject(request, ReplyStatus::Rejected,
                      QStringLiteral("staging-registration-failed"), error);
    }
    auto reply = submit(request);
    if (!reply.committed()) {
        QString rollbackError;
        if (!unregisterContainer(request.containerId, &rollbackError)) {
            reply.failure.message.append(
                QStringLiteral("; staging rollback failed: %1").arg(rollbackError));
        }
    }
    return reply;
}

ControlReply ContainerControlBridge::reject(const ControlRequest &request,
                                            ReplyStatus status,
                                            QString code,
                                            QString message,
                                            qsizetype operationIndex,
                                            quint64 currentRevision) const
{
    return {{},
            request.transactionId,
            request.containerId,
            status,
            currentRevision,
            {},
            {std::move(code), std::move(message), operationIndex}};
}

ControlReply ContainerControlBridge::submit(const ControlRequest &request)
{
    assertThread();
    if (request.transactionId.isEmpty() || request.containerId.isEmpty()
        || request.operations.isEmpty()) {
        return reject(request, ReplyStatus::Rejected,
                      QStringLiteral("malformed-request"),
                      QStringLiteral("transactionId, containerId, and operations must be non-empty"));
    }
    if (request.protocol.major != ProtocolVersion::CurrentMajor
        || request.protocol.minor > ProtocolVersion::CurrentMinor) {
        return reject(request, ReplyStatus::Rejected,
                      QStringLiteral("unsupported-protocol"),
                      QStringLiteral("supported protocol is %1.%2")
                          .arg(ProtocolVersion::CurrentMajor)
                          .arg(ProtocolVersion::CurrentMinor));
    }
    if (m_applying) {
        return reject(request, ReplyStatus::Rejected,
                      QStringLiteral("transaction-in-progress"),
                      QStringLiteral("nested compositor transactions are not allowed"));
    }
    auto state = m_containers.find(request.containerId);
    if (state == m_containers.end()) {
        return reject(request, ReplyStatus::Rejected,
                      QStringLiteral("unknown-container"),
                      QStringLiteral("unknown container '%1'").arg(request.containerId));
    }
    if (request.expectedRevision != state->revision) {
        return reject(request, ReplyStatus::Conflict,
                      QStringLiteral("revision-conflict"),
                      QStringLiteral("expected revision %1 but current revision is %2")
                          .arg(request.expectedRevision)
                          .arg(state->revision),
                      -1, state->revision);
    }
    if (state->revision == std::numeric_limits<quint64>::max()) {
        return reject(request, ReplyStatus::Rejected,
                      QStringLiteral("revision-exhausted"),
                      QStringLiteral("container revision cannot be incremented"),
                      -1, state->revision);
    }

    // AGENT-GUARD: Keep the guard active through signal delivery. Direct slots
    // may inspect committed state, but a nested mutation would invalidate the
    // current QHash iterator and violate one-revision-per-transaction ordering.
    QScopedValueRollback<bool> applying(m_applying, true);
    auto candidate = state->container;
    for (qsizetype index = 0; index < request.operations.size(); ++index) {
        QString code;
        QString error;
        if (!applyOperation(candidate, request.operations[index], &code, &error)) {
            return reject(request, ReplyStatus::Rejected, std::move(code), std::move(error),
                          index, state->revision);
        }
    }
    // AGENT-CONTRACT: Singleton containers are valid only as unpublished
    // staging state. When a mutation reduces a real group to one member,
    // unwrap the survivor in the same scene transaction so no caller can
    // observe a stranded one-window container.
    if (!state->container.singleWindowId() && candidate.singleWindowId()) {
        QString unwrapError;
        const auto survivor = *candidate.singleWindowId();
        if (!candidate.detachWindow(survivor, &unwrapError)) {
            return reject(request, ReplyStatus::Rejected,
                          QStringLiteral("unwrap-failed"), unwrapError,
                          -1, state->revision);
        }
    }
    const auto validation = candidate.validate();
    if (!validation.valid) {
        return reject(request, ReplyStatus::Rejected,
                      QStringLiteral("invalid-candidate"), validation.message,
                      -1, state->revision);
    }

    QString sceneError;
    auto sceneTransaction = m_adapter.prepareTransition(state->container, candidate, &sceneError);
    if (!sceneTransaction) {
        return reject(request, ReplyStatus::Rejected,
                      QStringLiteral("scene-prepare-failed"),
                      sceneError.isEmpty() ? QStringLiteral("scene adapter rejected transition")
                                           : sceneError,
                      -1, state->revision);
    }
    if (!sceneTransaction->commit(&sceneError)) {
        return reject(request, ReplyStatus::Rejected,
                      QStringLiteral("scene-commit-failed"),
                      sceneError.isEmpty() ? QStringLiteral("scene transaction failed and rolled back")
                                           : sceneError,
                      -1, state->revision);
    }

    state->container = std::move(candidate);
    ++state->revision;
    const auto committedSnapshot = state->container.toJson();
    const auto committedRevision = state->revision;
    Q_EMIT containerCommitted(request.containerId, committedRevision, committedSnapshot);
    if (state->container.pages().isEmpty()) {
        m_containers.erase(state);
    }
    return {{},
            request.transactionId,
            request.containerId,
            ReplyStatus::Committed,
            committedRevision,
            committedSnapshot,
            {}};
}

} // namespace QindaQt::Compositor
