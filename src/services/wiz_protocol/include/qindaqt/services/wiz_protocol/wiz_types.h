// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QFlags>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace QindaQt::Wiz
{

enum class Availability : quint32 {
    Starting = 0,
    Ready = 1,
    Unavailable = 2,
    Degraded = 3,
};

// How recently the luminaire answered. A light that stops answering is kept in
// the inventory as Unreachable rather than disappearing, so a stored label and
// its presets survive a power cut or a temporary Wi-Fi drop.
enum class Reachability : quint32 {
    Unknown = 0,
    Online = 1,
    Stale = 2,
    Unreachable = 3,
};

// What the hardware can actually do, derived from the device's own module name
// and model configuration.
//
// AGENT-GUARD: never infer a feature from the current pilot state. A light
// that happens to report r/g/b values is not proof of a colour channel; only
// the model configuration and module name are evidence.
enum class Feature : quint32 {
    None = 0,
    Power = 1U << 0U,
    Dimming = 1U << 1U,
    ColorTemperature = 1U << 2U,
    Color = 1U << 3U,
    Scenes = 1U << 4U,
    SceneSpeed = 1U << 5U,
    DualHeadRatio = 1U << 6U,
};
Q_DECLARE_FLAGS(Features, Feature)

// Which colour channel set the luminaire is currently following. Wiz firmware
// answers one pilot for all three, so the mode is derived once here instead of
// by every consumer.
enum class LightMode : quint32 {
    Unknown = 0,
    Color = 1,
    White = 2,
    Scene = 3,
};

enum class OperationKind : quint32 {
    SetPower = 0,
    SetBrightness = 1,
    SetTemperature = 2,
    SetColor = 3,
    SetScene = 4,
    SetSpeed = 5,
    ApplyPreset = 6,
    Refresh = 7,
    Discover = 8,
};

enum class OperationStatus : quint32 {
    Succeeded = 0,
    // The request was well formed but this luminaire cannot honour it.
    Unsupported = 1,
    // The request was refused locally before any datagram was sent.
    Rejected = 2,
    // The device answered with an error object.
    Failed = 3,
    // No answer arrived before the deadline. The light may or may not have
    // applied the change; the caller must not replay it automatically.
    Uncertain = 4,
    Busy = 5,
};

// Identity is the device MAC exactly as the firmware reports it. The IP address
// is routing information only: DHCP may move a light between addresses without
// changing which luminaire it is.
struct DeviceIdentity {
    QString mac;
    QString address;
    quint16 port = 0;
    QString moduleName;
    QString firmwareVersion;
    quint32 homeId = 0;
    quint32 roomId = 0;

    friend bool operator==(const DeviceIdentity &, const DeviceIdentity &) = default;
};

struct DimmingRange {
    int minimumPercent = 10;
    int maximumPercent = 100;

    friend bool operator==(const DimmingRange &, const DimmingRange &) = default;
};

struct TemperatureRange {
    int minimumKelvin = 0;
    int maximumKelvin = 0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return minimumKelvin > 0 && maximumKelvin > minimumKelvin;
    }

    friend bool operator==(const TemperatureRange &, const TemperatureRange &) = default;
};

// The last pilot the device reported. Every field carries its own "known" bit:
// firmware omits members that do not apply to the active mode, and a missing
// member must never read as zero.
struct PilotState {
    bool on = false;
    bool dimmingKnown = false;
    quint8 dimmingPercent = 0;
    bool temperatureKnown = false;
    quint16 temperatureKelvin = 0;
    bool colorKnown = false;
    quint8 red = 0;
    quint8 green = 0;
    quint8 blue = 0;
    quint8 coolWhite = 0;
    quint8 warmWhite = 0;
    bool sceneKnown = false;
    quint16 sceneId = 0;
    bool speedKnown = false;
    quint8 speedPercent = 0;
    bool signalKnown = false;
    qint16 signalDbm = 0;

    [[nodiscard]] LightMode mode() const noexcept;

    friend bool operator==(const PilotState &, const PilotState &) = default;
};

// A partial setPilot intent. Only the fields whose `set*` bit is true are put
// on the wire.
//
// AGENT-CONTRACT: the vendor firmware rejects a setPilot that mixes colour,
// white-temperature, and scene selection. Validation collapses a request to one
// colour lane before encoding; see wiz_validation.h.
struct StateRequest {
    bool setPower = false;
    bool on = false;
    bool setDimming = false;
    quint8 dimmingPercent = 0;
    bool setTemperature = false;
    quint16 temperatureKelvin = 0;
    bool setColor = false;
    quint8 red = 0;
    quint8 green = 0;
    quint8 blue = 0;
    quint8 coolWhite = 0;
    quint8 warmWhite = 0;
    bool setScene = false;
    quint16 sceneId = 0;
    bool setSpeed = false;
    quint8 speedPercent = 0;

    [[nodiscard]] bool isEmpty() const noexcept
    {
        return !setPower && !setDimming && !setTemperature && !setColor
            && !setScene && !setSpeed;
    }

    friend bool operator==(const StateRequest &, const StateRequest &) = default;
};

struct Device {
    DeviceIdentity identity;
    // Human-readable name. A stored label wins; otherwise it is derived from
    // the model name and the MAC suffix.
    QString label;
    bool labelStored = false;
    Features features;
    DimmingRange dimming;
    TemperatureRange temperature;
    // False until the device has answered getModelConfig: capability-dependent
    // controls stay unavailable rather than guessed.
    bool capabilitiesKnown = false;
    bool pilotKnown = false;
    PilotState pilot;
    Reachability reachability = Reachability::Unknown;
    // Consecutive unanswered polls. Drives the reachability ladder.
    quint32 missedPolls = 0;

    friend bool operator==(const Device &, const Device &) = default;
};

struct Snapshot {
    quint32 schemaVersion = 1;
    quint64 epoch = 0;
    quint64 revision = 0;
    Availability availability = Availability::Starting;
    bool discovering = false;
    QString reasonCode;
    QString diagnostic;
    // Ordered by label, then MAC, so a projection is stable across revisions.
    QList<Device> devices;

    friend bool operator==(const Snapshot &, const Snapshot &) = default;
};

struct OperationRequest {
    OperationKind kind = OperationKind::Refresh;
    // Empty targets every known device; otherwise the exact device MAC.
    QString targetMac;
    StateRequest state;

    friend bool operator==(const OperationRequest &, const OperationRequest &) = default;
};

struct OperationResult {
    OperationKind kind = OperationKind::Refresh;
    OperationStatus status = OperationStatus::Failed;
    QString targetMac;
    quint64 initiatingEpoch = 0;
    quint64 initiatingRevision = 0;
    quint64 observedEpoch = 0;
    quint64 observedRevision = 0;
    QString reasonCode;
    QString diagnostic;

    friend bool operator==(const OperationResult &, const OperationResult &) = default;
};

// Normalizes a device-reported MAC to lowercase hexadecimal without
// separators, or returns an empty string when it is not a 12-digit MAC.
[[nodiscard]] QString normalizeMac(const QString &value);

// The name shown when no label has been stored: the model's marketing family
// and the last three MAC octets, which is what is printed on the device.
[[nodiscard]] QString derivedLabel(const DeviceIdentity &identity);

} // namespace QindaQt::Wiz

Q_DECLARE_OPERATORS_FOR_FLAGS(QindaQt::Wiz::Features)
Q_DECLARE_METATYPE(QindaQt::Wiz::Features)
Q_DECLARE_METATYPE(QindaQt::Wiz::PilotState)
Q_DECLARE_METATYPE(QindaQt::Wiz::Device)
Q_DECLARE_METATYPE(QindaQt::Wiz::Snapshot)
Q_DECLARE_METATYPE(QindaQt::Wiz::StateRequest)
Q_DECLARE_METATYPE(QindaQt::Wiz::OperationRequest)
Q_DECLARE_METATYPE(QindaQt::Wiz::OperationResult)
