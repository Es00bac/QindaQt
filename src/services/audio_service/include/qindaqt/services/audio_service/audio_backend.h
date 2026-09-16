// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_types.h>

#include <QtCore/QList>

#include <QtCore/QObject>

namespace QindaQt::Audio
{

enum class BackendOperationStatus {
    Succeeded,
    Unsupported,
    Failed,
    Uncertain,
};

struct BackendOperationOutcome {
    BackendOperationStatus status = BackendOperationStatus::Failed;
    QString reasonCode;
    QString diagnostic;
};

// One connection the console asks the graph to carry (ADR-0173). The console
// owns whether a connection should exist and at what level; the backend owns
// how that becomes PipeWire objects.
struct BackendRoutingEdge {
    // Stable console identities, not graph handles: the backend resolves them
    // against whatever nodes currently exist.
    QString stripId;
    QString busId;
    // The devices the console has bound to that strip and bus. An invalid
    // handle means the console knows the endpoint but the graph does not have
    // it right now, and the edge simply cannot be carried yet.
    Handle source;
    Handle target;
    double gainDb = 0.0;
    // False while the strip is muted, or silenced by another strip's solo, or
    // its destination bus is muted. The connection STAYS; only its level goes
    // to silence, so unmuting is instant instead of a graph rebuild.
    bool audible = true;

    friend bool operator==(const BackendRoutingEdge &,
                           const BackendRoutingEdge &) = default;
};

// AGENT-CONTRACT: Implementations receive requests on the Qt main thread and
// publish only immutable value copies through these signals. start() returns a
// fresh nonzero generation before that run can publish; every value carries the
// generation that produced it, and generations are equality tokens rather than
// ordered values. Within one resident backend object, snapshot epochs strictly
// increase whenever authority is replaced; an owner/process replacement starts
// a separate lineage. A platform adapter must not expose thread-affine handles
// through this boundary. See ADR-0014.
class AudioBackend : public QObject
{
    Q_OBJECT

public:
    explicit AudioBackend(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    ~AudioBackend() override = default;

    [[nodiscard]] virtual quint64 start() = 0;
    virtual void stop() = 0;
    virtual void submit(quint64 operationId, const OperationRequest &request) = 0;
    // Declares the complete set of connections the console currently wants.
    // The backend diffs this against what it has built and applies exactly the
    // difference; an implementation with no graph of its own may ignore it.
    //
    // AGENT-CONTRACT: this is declarative on purpose. A console change is one
    // statement of intent, not a sequence of add/remove commands, so a lost or
    // reordered call can never leave the graph half-wired.
    virtual void applyRouting(const QList<BackendRoutingEdge> &edges)
    {
        Q_UNUSED(edges)
    }

Q_SIGNALS:
    void snapshotReady(quint64 generation,
                       const QindaQt::Audio::Snapshot &snapshot);
    void operationFinished(quint64 generation, quint64 operationId,
                           const QindaQt::Audio::BackendOperationOutcome &outcome);
};

} // namespace QindaQt::Audio

Q_DECLARE_METATYPE(QindaQt::Audio::BackendOperationOutcome)
