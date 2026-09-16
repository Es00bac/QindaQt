// SPDX-License-Identifier: LGPL-3.0-or-later

// The console half of the Settings Audio route (ADR-0173): the read-only
// projection QML draws and the intents its controls dispatch. Split from
// audio_settings_projection.cpp so the console's own concerns read together and
// neither file outgrows its source-shape budget.

#include "qindaqt/apps/settings_audio/audio_settings_model.h"

#include <qindaqt/services/audio_protocol/audio_gain.h>

#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsAudio {
namespace {

using namespace QindaQt::Audio;

[[nodiscard]] QVariantMap levelMap(const Level &level)
{
    return QVariantMap{{QStringLiteral("peakDb"), level.peakDb},
                       {QStringLiteral("rmsDb"), level.rmsDb},
                       {QStringLiteral("known"), level.known}};
}

} // namespace

bool AudioSettingsModel::consoleAvailable() const
{
    const Snapshot snapshot = m_client.snapshot();
    // A console with no strips is not a console; a client must not draw an
    // empty rack as though the feature were present but unconfigured.
    return snapshot.capabilities.testFlag(Capability::Console)
        && !snapshot.console.strips.isEmpty();
}

bool AudioSettingsModel::consoleSoloActive() const
{
    return m_client.snapshot().console.soloActive;
}

QVariantList AudioSettingsModel::consoleStrips() const
{
    const Snapshot snapshot = m_client.snapshot();
    QVariantList rows;
    rows.reserve(snapshot.console.strips.size());
    for (const Strip &strip : snapshot.console.strips) {
        QVariantList sends;
        sends.reserve(strip.sends.size());
        for (const MatrixSend &send : strip.sends) {
            sends.append(QVariantMap{
                {QStringLiteral("busIndex"), send.busIndex},
                {QStringLiteral("enabled"), send.enabled},
                {QStringLiteral("gainDb"), send.gainDb}});
        }
        rows.append(QVariantMap{
            {QStringLiteral("id"), strip.id},
            {QStringLiteral("label"), strip.label},
            {QStringLiteral("virtual"), strip.kind == StripKind::VirtualInput},
            {QStringLiteral("gainDb"), strip.gainDb},
            {QStringLiteral("faderPosition"), faderPositionFromGainDb(strip.gainDb)},
            {QStringLiteral("muted"), strip.muted},
            {QStringLiteral("soloed"), strip.soloed},
            {QStringLiteral("mono"), strip.mono},
            {QStringLiteral("pan"), strip.pan},
            // A strip whose device is absent stays on the console and stays
            // configurable; it is drawn as unbound rather than removed, so the
            // user's routing does not vanish with the hardware.
            {QStringLiteral("bound"), strip.sourceKnown},
            // The device the strip follows right now, and whether that is the
            // user's pin or the automatic choice (ADR-0178). A picker shows
            // the pin when there is one, so an absent pinned device reads as
            // "this microphone, unplugged" rather than as "nothing".
            {QStringLiteral("sourceSerial"), strip.sourceKnown ? strip.sourceSerial : 0},
            {QStringLiteral("pinned"), !strip.pinnedSource.isEmpty()},
            {QStringLiteral("pinnedSource"), strip.pinnedSource},
            {QStringLiteral("sends"), sends},
            {QStringLiteral("level"), levelMap(strip.level)}});
    }
    return rows;
}

QVariantMap AudioSettingsModel::consoleLevels() const
{
    const Snapshot snapshot = m_client.snapshot();
    QVariantMap levels;
    for (const Strip &strip : snapshot.console.strips) {
        levels.insert(strip.id, levelMap(strip.level));
    }
    for (const Bus &bus : snapshot.console.buses) {
        levels.insert(bus.id, levelMap(bus.level));
    }
    return levels;
}

QVariantList AudioSettingsModel::consoleBuses() const
{
    const Snapshot snapshot = m_client.snapshot();
    QVariantList rows;
    rows.reserve(snapshot.console.buses.size());
    for (const Bus &bus : snapshot.console.buses) {
        rows.append(QVariantMap{
            {QStringLiteral("id"), bus.id},
            {QStringLiteral("index"), bus.index},
            {QStringLiteral("label"), bus.label},
            {QStringLiteral("virtual"), bus.kind == BusKind::Virtual},
            {QStringLiteral("gainDb"), bus.gainDb},
            {QStringLiteral("faderPosition"), faderPositionFromGainDb(bus.gainDb)},
            {QStringLiteral("muted"), bus.muted},
            {QStringLiteral("mono"), bus.mono},
            {QStringLiteral("bound"), bus.targetKnown},
            {QStringLiteral("targetSerial"), bus.targetKnown ? bus.targetSerial : 0},
            {QStringLiteral("pinned"), !bus.pinnedTarget.isEmpty()},
            {QStringLiteral("pinnedTarget"), bus.pinnedTarget},
            {QStringLiteral("level"), levelMap(bus.level)}});
    }
    return rows;
}

double AudioSettingsModel::faderPositionForGain(const double gainDb) const
{
    return faderPositionFromGainDb(gainDb);
}

double AudioSettingsModel::gainForFaderPosition(const double position) const
{
    return gainDbFromFaderPosition(position);
}

double AudioSettingsModel::unityFaderPosition() const
{
    return QindaQt::Audio::unityFaderPosition();
}

bool AudioSettingsModel::setStripFader(QString stripId, const double position)
{
    return dispatchConsoleIntent(ConsoleIntent::StripGain, std::move(stripId), 0,
                                 gainDbFromFaderPosition(position), false);
}

bool AudioSettingsModel::setStripSource(QString stripId, const quint64 serial)
{
    return dispatchPin(true, std::move(stripId), serial);
}

bool AudioSettingsModel::setBusTarget(QString busId, const quint64 serial)
{
    return dispatchPin(false, std::move(busId), serial);
}

bool AudioSettingsModel::dispatchPin(const bool strip, QString consoleId,
                                     const quint64 serial)
{
    // Same gate as every console intent: a pin re-routes, so it needs the
    // routing capability.
    const Snapshot snapshot = m_client.snapshot();
    if (!consoleAvailable()
        || !snapshot.capabilities.testFlag(Capability::SetConsoleRouting)) {
        rejectAction(QStringLiteral("unsupported"));
        return false;
    }
    if (consoleId.isEmpty()) {
        rejectAction(QStringLiteral("stale-handle"));
        return false;
    }
    // Serial 0 is "automatic": sent as an invalid handle, which the service
    // reads as clearing the pin (ADR-0178).
    const Handle device = serial == 0 ? Handle{} : Handle{snapshot.epoch, serial};
    const quint64 requestId = strip ? m_client.setStripSource(consoleId, device)
                                    : m_client.setBusTarget(consoleId, device);
    if (requestId == 0) {
        rejectAction(QString());
        return false;
    }
    return true;
}

bool AudioSettingsModel::setStripMuted(QString stripId, const bool muted)
{
    return dispatchConsoleIntent(ConsoleIntent::StripMute, std::move(stripId), 0,
                                 0.0, muted);
}

bool AudioSettingsModel::setStripSoloed(QString stripId, const bool soloed)
{
    return dispatchConsoleIntent(ConsoleIntent::StripSolo, std::move(stripId), 0,
                                 0.0, soloed);
}

bool AudioSettingsModel::setStripMono(QString stripId, const bool mono)
{
    return dispatchConsoleIntent(ConsoleIntent::StripMono, std::move(stripId), 0,
                                 0.0, mono);
}

bool AudioSettingsModel::setStripPan(QString stripId, const double pan)
{
    return dispatchConsoleIntent(ConsoleIntent::StripPan, std::move(stripId), 0,
                                 pan, false);
}

bool AudioSettingsModel::setStripSend(QString stripId, const int busIndex,
                                      const bool enabled, const double gainDb)
{
    if (busIndex < 0) {
        return false;
    }
    return dispatchConsoleIntent(ConsoleIntent::StripSend, std::move(stripId),
                                 static_cast<quint32>(busIndex), gainDb, enabled);
}

bool AudioSettingsModel::setBusFader(QString busId, const double position)
{
    return dispatchConsoleIntent(ConsoleIntent::BusGain, std::move(busId), 0,
                                 gainDbFromFaderPosition(position), false);
}

bool AudioSettingsModel::setBusMuted(QString busId, const bool muted)
{
    return dispatchConsoleIntent(ConsoleIntent::BusMute, std::move(busId), 0, 0.0,
                                 muted);
}

bool AudioSettingsModel::setBusMono(QString busId, const bool mono)
{
    return dispatchConsoleIntent(ConsoleIntent::BusMono, std::move(busId), 0, 0.0,
                                 mono);
}

bool AudioSettingsModel::dispatchConsoleIntent(const ConsoleIntent intent,
                                               QString consoleId,
                                               const quint32 busIndex,
                                               const double value,
                                               const bool flag)
{
    // AGENT-GUARD: the same predicate that enables the control. Routing needs
    // the routing capability and everything else needs the gain capability, so
    // a service publishing a read-only console cannot be driven from here.
    const Capability capability = intent == ConsoleIntent::StripSend
        ? Capability::SetConsoleRouting
        : Capability::SetConsoleGain;
    const Snapshot snapshot = m_client.snapshot();
    if (!consoleAvailable() || !snapshot.capabilities.testFlag(capability)) {
        rejectAction(QStringLiteral("unsupported"));
        return false;
    }
    if (consoleId.isEmpty()) {
        rejectAction(QStringLiteral("stale-handle"));
        return false;
    }
    quint64 requestId = 0;
    switch (intent) {
    case ConsoleIntent::StripGain:
        requestId = m_client.setStripGain(consoleId, value);
        break;
    case ConsoleIntent::StripMute:
        requestId = m_client.setStripMuted(consoleId, flag);
        break;
    case ConsoleIntent::StripSolo:
        requestId = m_client.setStripSoloed(consoleId, flag);
        break;
    case ConsoleIntent::StripMono:
        requestId = m_client.setStripMono(consoleId, flag);
        break;
    case ConsoleIntent::StripPan:
        requestId = m_client.setStripPan(consoleId, value);
        break;
    case ConsoleIntent::StripSend:
        requestId = m_client.setStripSend(consoleId, busIndex, flag, value);
        break;
    case ConsoleIntent::BusGain:
        requestId = m_client.setBusGain(consoleId, value);
        break;
    case ConsoleIntent::BusMute:
        requestId = m_client.setBusMuted(consoleId, flag);
        break;
    case ConsoleIntent::BusMono:
        requestId = m_client.setBusMono(consoleId, flag);
        break;
    }
    if (requestId == 0) {
        rejectAction(QString());
        return false;
    }
    return true;
}

} // namespace QindaQt::Apps::SettingsAudio
