// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The shared fixture for the controller suites: a fake Audio1 transport and the
// bounded snapshot every row projects from. Two suites use it — the projection
// and lifecycle rows in tst_audio_applet_controller.cpp, and the ADR-0191
// control-ownership rows in tst_audio_applet_coalescing.cpp — which is why it
// lives in a named namespace rather than an anonymous one.

#include "audio_applet_controller.h"

#include <qindaqt/services/audio_client/audio_client.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <QSignalSpy>
#include <QtTest>

#include <limits>

namespace QindaQt::Shell::AudioApplet::Tests {

using QindaQt::Audio::AudioClient;
using QindaQt::Audio::AudioTransport;
using QindaQt::Audio::Availability;
using QindaQt::Audio::Capability;
using QindaQt::Audio::Capabilities;
using QindaQt::Audio::Device;
using QindaQt::Audio::DeviceKind;
using QindaQt::Audio::Handle;
using QindaQt::Audio::OperationKind;
using QindaQt::Audio::OperationRequest;
using QindaQt::Audio::OperationResult;
using QindaQt::Audio::OperationStatus;
using QindaQt::Audio::Snapshot;
using QindaQt::Audio::Stream;
using QindaQt::Audio::StreamDirection;

inline constexpr quint64 kEpoch = 11;
inline constexpr quint64 kRevision = 4;
inline const QString kOwner = QStringLiteral(":1.42");

inline Handle handleFor(quint64 serial)
{
    return Handle{.epoch = kEpoch, .serial = serial};
}

inline Device makeDevice(quint64 serial, DeviceKind kind, const QString &description,
                  bool defaultDevice)
{
    Device device;
    device.handle = handleFor(serial);
    device.kind = kind;
    device.name = QStringLiteral("dev%1").arg(serial);
    device.description = description;
    device.volume = 0.5;
    device.volumeKnown = true;
    device.muted = false;
    device.muteKnown = true;
    device.isDefault = defaultDevice;
    device.canSetVolume = true;
    device.canSetMute = true;
    return device;
}

// One default output (serial 1), one limited input (serial 2: unknown volume,
// no volume control), one full default input (serial 3), and one playback
// stream (serial 4) targeting the default output. Serials strictly ascend
// across all lists, as Audio1 validation requires.
inline Snapshot makeReadySnapshot()
{
    Snapshot snapshot;
    snapshot.schemaVersion = QindaQt::Audio::kSchemaVersion;
    snapshot.epoch = kEpoch;
    snapshot.revision = kRevision;
    snapshot.availability = Availability::Ready;
    snapshot.capabilities = Capabilities(Capability::SetVolume)
        | Capability::SetMute;

    Device output = makeDevice(1, DeviceKind::Output,
                               QStringLiteral("Built-in Speakers"), true);
    snapshot.defaultOutput = output.handle;
    snapshot.outputs.append(output);

    Device limitedInput = makeDevice(2, DeviceKind::Input,
                                     QStringLiteral("Limited Microphone"),
                                     false);
    limitedInput.volume = 0.0;
    limitedInput.volumeKnown = false;
    limitedInput.canSetVolume = false;
    snapshot.inputs.append(limitedInput);

    Device input = makeDevice(3, DeviceKind::Input,
                              QStringLiteral("Webcam Microphone"), true);
    snapshot.defaultInput = input.handle;
    snapshot.inputs.append(input);

    Stream stream;
    stream.handle = handleFor(4);
    stream.direction = StreamDirection::Playback;
    stream.applicationName = QStringLiteral("Music Player");
    stream.mediaName = QStringLiteral("Song");
    stream.target = handleFor(1);
    stream.targetKnown = true;
    stream.volume = 0.4;
    stream.volumeKnown = true;
    stream.muted = false;
    stream.muteKnown = true;
    stream.canSetVolume = true;
    stream.canSetMute = true;
    stream.canMove = false;
    snapshot.streams.append(stream);

    return snapshot;
}

// Re-stamp every handle in the snapshot to the given epoch so a new lineage
// stays internally consistent under Audio1 validation.
Snapshot withEpoch(Snapshot snapshot, quint64 epoch)
{
    snapshot.epoch = epoch;
    snapshot.revision = 1;
    for (Device &device : snapshot.outputs)
        device.handle.epoch = epoch;
    for (Device &device : snapshot.inputs)
        device.handle.epoch = epoch;
    for (Stream &stream : snapshot.streams) {
        stream.handle.epoch = epoch;
        if (stream.targetKnown)
            stream.target.epoch = epoch;
    }
    snapshot.defaultOutput.epoch = epoch;
    snapshot.defaultInput.epoch = epoch;
    return snapshot;
}

inline OperationResult makeResult(OperationKind kind, OperationStatus status,
                           const QString &reasonCode)
{
    return {.kind = kind,
            .status = status,
            .initiatingEpoch = kEpoch,
            .initiatingRevision = kRevision,
            .observedEpoch = kEpoch,
            .observedRevision = kRevision,
            .reasonCode = reasonCode,
            .diagnostic = {},
            .wireValid = true};
}

class FakeTransport final : public AudioTransport {
public:
    explicit FakeTransport(QObject *parent = nullptr) : AudioTransport(parent) {}

    struct Fetch {
        QString owner;
        quint64 requestId = 0;
    };
    struct Submission {
        QString owner;
        quint64 requestId = 0;
        OperationRequest request;
    };

    void start() override { m_started = true; }
    void stop() override { m_stopped = true; }
    void fetchSnapshot(const QString &owner, quint64 requestId) override
    {
        fetches.append(Fetch{owner, requestId});
    }
    void submitOperation(const QString &owner, quint64 requestId,
                         const OperationRequest &request) override
    {
        submissions.append(Submission{owner, requestId, request});
    }

    void changeOwner(const QString &owner) { Q_EMIT ownerChanged(owner); }
    void invalidate(quint64 epoch, quint64 revision)
    {
        Q_EMIT invalidated(kOwner, epoch, revision);
    }
    void deliverSnapshot(quint64 requestId, bool success,
                         const Snapshot &snapshot, const QString &reason = {})
    {
        Q_EMIT snapshotReply(kOwner, requestId, success, snapshot, reason);
    }
    void deliverSnapshotAs(const QString &owner, quint64 requestId, bool success,
                           const Snapshot &snapshot,
                           const QString &reason = {})
    {
        Q_EMIT snapshotReply(owner, requestId, success, snapshot, reason);
    }
    void deliverOperation(quint64 requestId, bool success,
                          const OperationResult &result,
                          const QString &reason = {})
    {
        Q_EMIT operationReply(kOwner, requestId, success, result, reason);
    }

    bool m_started = false;
    bool m_stopped = false;
    QList<Fetch> fetches;
    QList<Submission> submissions;
};

} // namespace QindaQt::Shell::AudioApplet::Tests

namespace QindaQt::Shell::AudioApplet::Tests {

// Owns one controller over one fake transport for the length of a row.
//
// AGENT-NOTE: deliberately no Q_OBJECT. This base declares no signals or slots
// of its own — each suite declares its own — and a Q_OBJECT in a header that is
// not itself a build source leaves every including target without the moc
// output for it.
class ControllerFixture : public QObject {
protected:
    void createController()
    {
        m_transport = new FakeTransport(this);
        m_client = new AudioClient(m_transport, this);
        m_controller = new AudioAppletController(m_client, true, true, this);
    }

    void destroyController()
    {
        delete m_controller;
        m_controller = nullptr;
        delete m_client;
        m_client = nullptr;
        delete m_transport;
        m_transport = nullptr;
    }

    void publishReadySnapshot()
    {
        m_client->start();
        m_transport->changeOwner(kOwner);
        QVERIFY(!m_transport->fetches.isEmpty());
        m_transport->deliverSnapshot(m_transport->fetches.constLast().requestId,
                                     true, makeReadySnapshot());
        QCOMPARE(m_controller->phaseText(), QStringLiteral("ready"));
    }

    // A revision or epoch change reaches the client as an invalidation, which
    // starts a new fetch that must be answered for the snapshot to publish.
    void deliverSnapshotAfterRefetch(const Snapshot &snapshot)
    {
        QVERIFY(!m_transport->fetches.isEmpty());
        m_transport->deliverSnapshot(m_transport->fetches.constLast().requestId,
                                     true, snapshot);
    }

    [[nodiscard]] int countPendingDeviceRows() const
    {
        int pending = 0;
        for (const QVariant &value : m_controller->deviceRows()) {
            if (value.value<DeviceRow>().pending())
                ++pending;
        }
        return pending;
    }

    [[nodiscard]] int countPendingStreamRows() const
    {
        int pending = 0;
        for (const QVariant &value : m_controller->streamRows()) {
            if (value.value<StreamRow>().pending())
                ++pending;
        }
        return pending;
    }

    [[nodiscard]] DeviceRow deviceRowFor(const quint64 serial) const
    {
        for (const QVariant &value : m_controller->deviceRows()) {
            const auto row = value.value<DeviceRow>();
            if (row.serial() == serial)
                return row;
        }
        return {};
    }

    FakeTransport *m_transport = nullptr;
    AudioClient *m_client = nullptr;
    AudioAppletController *m_controller = nullptr;
};

} // namespace QindaQt::Shell::AudioApplet::Tests
