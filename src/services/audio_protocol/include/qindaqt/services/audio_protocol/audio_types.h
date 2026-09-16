// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_console.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <QtCore/QFlags>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace QindaQt::Audio
{

enum class Availability : quint32 {
    Starting = 0,
    Ready = 1,
    Unavailable = 2,
    Degraded = 3,
};

enum class DeviceKind : quint32 {
    Output = 0,
    Input = 1,
};

enum class StreamDirection : quint32 {
    Playback = 0,
    Capture = 1,
};

enum class Capability : quint32 {
    None = 0,
    SetDefault = 1U << 0U,
    SetVolume = 1U << 1U,
    SetMute = 1U << 2U,
    MoveStream = 1U << 3U,
    SetChannelVolumes = 1U << 4U,
    ManageVirtualDevices = 1U << 5U,
    // Console slice (ADR-0173). A client that does not know these bits ignores
    // the console entirely and keeps working against devices and streams.
    Console = 1U << 6U,
    SetConsoleGain = 1U << 7U,
    SetConsoleRouting = 1U << 8U,
    ConsoleMeters = 1U << 9U,
};
Q_DECLARE_FLAGS(Capabilities, Capability)

enum class OperationKind : quint32 {
    SetDefault = 0,
    SetVolume = 1,
    SetMute = 2,
    MoveStream = 3,
    SetChannelVolumes = 4,
    CreateVirtualDevice = 5,
    RemoveVirtualDevice = 6,
    // AGENT-GUARD: these numbers are wire values. Append only; renumbering an
    // existing kind silently reinterprets an in-flight request from an older
    // client as a different operation.
    SetStripGain = 7,
    SetStripMute = 8,
    SetStripSolo = 9,
    SetStripMono = 10,
    SetStripPan = 11,
    SetStripTrim = 12,
    SetStripSend = 13,
    SetBusGain = 14,
    SetBusMute = 15,
    SetBusMono = 16,
    SetBusTarget = 17,
    // Pins a strip to a capture device by name (ADR-0178). An invalid handle
    // clears the pin and returns the strip to automatic binding.
    SetStripSource = 18,
    // Replaces a strip's whole processing rack (ADR-0179). One operation for
    // the four blocks: a client always holds the current rack and sends it
    // back with one control changed, which keeps the surface to one method.
    SetStripProcessing = 19,
};

enum class OperationStatus : quint32 {
    Succeeded = 0,
    Rejected = 1,
    Unsupported = 2,
    Failed = 3,
    Uncertain = 4,
    Busy = 5,
};

struct Handle {
    quint64 epoch = 0;
    quint64 serial = 0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return epoch != 0 && serial != 0;
    }

    friend bool operator==(const Handle &, const Handle &) = default;
};

struct Device {
    Handle handle;
    DeviceKind kind = DeviceKind::Output;
    QString name;
    QString description;
    double volume = 0.0;
    bool volumeKnown = false;
    bool muted = false;
    bool muteKnown = false;
    bool isDefault = false;
    bool canSetVolume = false;
    bool canSetMute = false;

    // Per-channel truth projected onto the node's channel map. When the map is
    // non-empty the adapter publishes exactly one volume per mapped channel;
    // entries the mixer has not reported yet carry the aggregate level.
    QVector<double> channelVolumes = {};
    QStringList channelMap = {};
    // True only for managed null devices whose node.name carries the virtual
    // prefix; such devices may be removed through RemoveVirtualDevice.
    bool virtualDevice = false;
    // The node's `node.name` (schema 4). `name` is for people and changes with
    // locale and user renames; this is for identity. It is what a console
    // stores to find the same device again after a reboot, when every serial
    // has changed. Appended last so the fields before it keep their wire order.
    // The default member initializer keeps every existing designated
    // initializer of Device valid under -Werror=missing-field-initializers.
    QString nodeName = {};

    // AGENT-GUARD: D-Bus decoding sets this false when a nested channel array
    // exceeded its bound while still consuming the complete argument. Snapshot
    // decoding folds it into Snapshot::wireValid; clients must validate before
    // publishing any decoded value.
    bool wireValid = true;

    friend bool operator==(const Device &, const Device &) = default;
};

struct Stream {
    Handle handle;
    StreamDirection direction = StreamDirection::Playback;
    QString applicationName;
    QString mediaName;
    Handle target;
    bool targetKnown = false;
    double volume = 0.0;
    bool volumeKnown = false;
    bool muted = false;
    bool muteKnown = false;
    bool canSetVolume = false;
    bool canSetMute = false;
    bool canMove = false;

    QVector<double> channelVolumes = {};
    QStringList channelMap = {};

    bool wireValid = true;

    friend bool operator==(const Stream &, const Stream &) = default;
};

struct Snapshot {
    quint32 schemaVersion = kSchemaVersion;
    quint64 epoch = 0;
    quint64 revision = 0;
    Availability availability = Availability::Starting;
    Capabilities capabilities;
    QString reasonCode;
    QString diagnostic;
    Handle defaultOutput;
    Handle defaultInput;
    QList<Device> outputs;
    QList<Device> inputs;
    QList<Stream> streams;
    // The mixing console (ADR-0173). Empty when the service publishes no
    // console, which is how a v3 service reports the S1-only state.
    Console console;

    // AGENT-GUARD: D-Bus decoding sets this false when an array exceeded its
    // bound while still consuming the complete argument. Clients must validate
    // it before publishing any decoded values.
    bool wireValid = true;

    friend bool operator==(const Snapshot &, const Snapshot &) = default;
};

struct OperationRequest {
    OperationKind kind = OperationKind::SetDefault;
    Handle primary;
    Handle secondary;
    double volume = 0.0;
    bool muted = false;

    // SetChannelVolumes: one volume per channel of the retained target layout.
    // CreateVirtualDevice: deviceKind, displayName and the requested channel
    // count (2, 4, 6 or 8). primary stays invalid for CreateVirtualDevice.
    QVector<double> channelVolumes = {};
    DeviceKind deviceKind = DeviceKind::Output;
    QString displayName = {};
    quint32 channels = 0;

    // Console operations (ADR-0173) address a strip or bus by its stable
    // console id rather than by a graph handle, so a request survives the
    // device behind it disappearing and returning.
    QString consoleId = {};
    // SetStripSend: the destination bus and whether the send is on.
    quint32 busIndex = 0;
    bool enabled = false;
    // Gain in dB for every console gain operation; pan for SetStripPan.
    double gainDb = 0.0;
    double pan = 0.0;
    // SetStripSource / SetBusTarget: the device's node.name, resolved by the
    // service from `primary` before the console model sees the request. Not a
    // wire field - the methods take a handle - which is why it is not part of
    // any marshalling.
    QString nodeName = {};
    // SetStripProcessing: the rack to apply.
    StripProcessing processing = {};

    friend bool operator==(const OperationRequest &, const OperationRequest &) = default;
};

struct OperationResult {
    OperationKind kind = OperationKind::SetDefault;
    OperationStatus status = OperationStatus::Failed;
    quint64 initiatingEpoch = 0;
    quint64 initiatingRevision = 0;
    quint64 observedEpoch = 0;
    quint64 observedRevision = 0;
    QString reasonCode;
    QString diagnostic;
    bool wireValid = true;

    friend bool operator==(const OperationResult &, const OperationResult &) = default;
};

} // namespace QindaQt::Audio

Q_DECLARE_OPERATORS_FOR_FLAGS(QindaQt::Audio::Capabilities)
Q_DECLARE_METATYPE(QindaQt::Audio::Availability)
Q_DECLARE_METATYPE(QindaQt::Audio::DeviceKind)
Q_DECLARE_METATYPE(QindaQt::Audio::StreamDirection)
Q_DECLARE_METATYPE(QindaQt::Audio::Capabilities)
Q_DECLARE_METATYPE(QindaQt::Audio::OperationKind)
Q_DECLARE_METATYPE(QindaQt::Audio::OperationStatus)
Q_DECLARE_METATYPE(QindaQt::Audio::Handle)
Q_DECLARE_METATYPE(QindaQt::Audio::Device)
Q_DECLARE_METATYPE(QindaQt::Audio::Stream)
Q_DECLARE_METATYPE(QindaQt::Audio::Snapshot)
Q_DECLARE_METATYPE(QindaQt::Audio::OperationRequest)
Q_DECLARE_METATYPE(QindaQt::Audio::OperationResult)
