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
    // True when the strip's source is a sink whose monitor carries the audio -
    // a virtual strip - rather than a capture device.
    bool sourceIsSink = false;
    double gainDb = 0.0;
    // -1.0 hard left to +1.0 hard right, applied as balance on the send's
    // two playback channels (ADR-0177).
    double pan = 0.0;
    // False while the strip is muted, or silenced by another strip's solo, or
    // its destination bus is muted. The connection STAYS; only its level goes
    // to silence, so unmuting is instant instead of a graph rebuild.
    bool audible = true;

    friend bool operator==(const BackendRoutingEdge &,
                           const BackendRoutingEdge &) = default;
};

// One console element the service wants a meter for (ADR-0174). Metering is
// declared separately from routing because a strip must show its input level
// even when the user has not assigned it to any bus yet.
struct BackendMeterTarget {
    QString consoleId;
    // The device to read. An invalid handle is not a target: the console knows
    // the endpoint but the graph does not have it right now.
    Handle device;
    // True when the audio must be taken from the device's monitor rather than
    // read as a capture source - virtual input strips and every bus are sinks.
    bool captureSink = false;
    // A passive meter reads only while something else already has the device
    // open. Metering an INPUT non-passively switches the user's microphone on
    // and lights their camera's recording indicator, so a capture meter stays
    // passive until the user has asked for that input to be live.
    bool passive = true;

    friend bool operator==(const BackendMeterTarget &,
                           const BackendMeterTarget &) = default;
};

// One graph endpoint a console's VIRTUAL strip or bus is made of (ADR-0175):
// the null sink an application plays into for a strip, or the sink-plus-source
// pair other applications record from for a bus. The backend creates whichever
// of these the daemon does not already have.
struct BackendConsoleEndpoint {
    QString consoleId;
    bool isBus = false;
    // What the user sees in every application's device picker.
    QString description;

    friend bool operator==(const BackendConsoleEndpoint &,
                           const BackendConsoleEndpoint &) = default;
};

// One strip's active processing rack (ADR-0179): the device to read, and the
// rack to run on it. Declared only for strips with at least one block enabled
// and a device the graph currently has.
struct BackendProcessingChain {
    QString stripId;
    Handle source;
    bool sourceIsSink = false;
    StripProcessing processing;

    friend bool operator==(const BackendProcessingChain &,
                           const BackendProcessingChain &) = default;
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
    // Declares the complete set of elements to meter. Declarative for the same
    // reason routing is: the caller states what should be metered, never what
    // to start or stop.
    virtual void applyMetering(const QList<BackendMeterTarget> &targets)
    {
        Q_UNUSED(targets)
    }
    // Declares the complete set of virtual endpoints the console needs to
    // exist. Declarative like routing: the console states what its virtual
    // strips and buses are, never "create this now".
    virtual void applyConsoleEndpoints(const QList<BackendConsoleEndpoint> &endpoints)
    {
        Q_UNUSED(endpoints)
    }
    // Declares every strip rack that should be running. Declarative like the
    // rest; a backend with no graph may ignore it.
    virtual void applyProcessing(const QList<BackendProcessingChain> &chains)
    {
        Q_UNUSED(chains)
    }

Q_SIGNALS:
    void snapshotReady(quint64 generation,
                       const QindaQt::Audio::Snapshot &snapshot);
    void operationFinished(quint64 generation, quint64 operationId,
                           const QindaQt::Audio::BackendOperationOutcome &outcome);
    // Meter readings, at meter rate. Deliberately not part of snapshotReady:
    // levels must not force a snapshot revision (see LevelReading).
    void levelsReady(quint64 generation,
                     const QList<QindaQt::Audio::LevelReading> &levels);
};

} // namespace QindaQt::Audio

Q_DECLARE_METATYPE(QindaQt::Audio::BackendOperationOutcome)
