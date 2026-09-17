// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/wiz_protocol/wiz_types.h>

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>

#include <optional>

namespace QindaQt::SmartLights
{

// What the user has told us about one luminaire. The device itself owns its
// state; this is the part that belongs to the desktop and must survive a
// restart, a power cut, and a DHCP address change.
struct StoredDevice {
    // Normalized device MAC. This, not the address, is the stored identity.
    QString mac;
    QString label;
    // Last address the light answered on, used to reach known lights
    // immediately at startup instead of waiting for a broadcast round.
    QString lastAddress;

    friend bool operator==(const StoredDevice &, const StoredDevice &) = default;
};

// One luminaire's part of a preset: the exact control intent to re-apply.
struct PresetMember {
    QString mac;
    Wiz::StateRequest state;

    friend bool operator==(const PresetMember &, const PresetMember &) = default;
};

// A named lighting arrangement captured from live devices and replayable onto
// them later.
//
// AGENT-CONTRACT: a preset stores intents, not observed pilots. Replaying it
// must produce the same request that was captured, so a firmware that reports
// its state differently than it accepts it cannot make a preset drift.
struct StoredPreset {
    QString id;
    QString name;
    QList<PresetMember> members;

    friend bool operator==(const StoredPreset &, const StoredPreset &) = default;
};

struct StoredConfiguration {
    quint32 schemaVersion = 1;
    QList<StoredDevice> devices;
    QList<StoredPreset> presets;

    friend bool operator==(const StoredConfiguration &, const StoredConfiguration &) = default;
};

[[nodiscard]] QByteArray encodeConfiguration(const StoredConfiguration &configuration);

// Returns nothing when the document is not a configuration this version
// understands. A malformed or future-version file is never partially adopted:
// silently keeping half of someone's presets would be worse than reporting
// that the file could not be read.
[[nodiscard]] std::optional<StoredConfiguration> decodeConfiguration(
    const QByteArray &document);

// Generates a stable identifier for a new preset from its name plus a
// disambiguating suffix, avoiding collisions with `existing`.
[[nodiscard]] QString makePresetId(const QString &name, const QList<StoredPreset> &existing);

} // namespace QindaQt::SmartLights
