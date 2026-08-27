// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QFlags>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>

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
};
Q_DECLARE_FLAGS(Capabilities, Capability)

enum class OperationKind : quint32 {
    SetDefault = 0,
    SetVolume = 1,
    SetMute = 2,
    MoveStream = 3,
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

    friend bool operator==(const Stream &, const Stream &) = default;
};

struct Snapshot {
    quint32 schemaVersion = 1;
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
