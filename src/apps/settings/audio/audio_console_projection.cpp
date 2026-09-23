// SPDX-License-Identifier: LGPL-3.0-or-later

// The console half of the Settings Audio route (ADR-0173): the read-only
// projection QML draws and the intents its controls dispatch. Split from
// audio_settings_projection.cpp so the console's own concerns read together and
// neither file outgrows its source-shape budget.

#include "qindaqt/apps/settings_audio/audio_settings_model.h"

#include <qindaqt/services/audio_protocol/audio_gain.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsAudio {
namespace {

using namespace QindaQt::Audio;

[[nodiscard]] QVariantMap equalizerMap(const EqualizerSettings &eq)
{
    return QVariantMap{{QStringLiteral("enabled"), eq.enabled},
                       {QStringLiteral("lowHz"), eq.lowHz},
                       {QStringLiteral("lowGainDb"), eq.lowGainDb},
                       {QStringLiteral("midHz"), eq.midHz},
                       {QStringLiteral("midGainDb"), eq.midGainDb},
                       {QStringLiteral("midQ"), eq.midQ},
                       {QStringLiteral("highHz"), eq.highHz},
                       {QStringLiteral("highGainDb"), eq.highGainDb}};
}

[[nodiscard]] QString busModeToken(const BusMode mode)
{
    switch (mode) {
    case BusMode::Normal:
        return QStringLiteral("normal");
    case BusMode::SwapChannels:
        return QStringLiteral("swap");
    case BusMode::LeftToBoth:
        return QStringLiteral("left");
    case BusMode::RightToBoth:
        return QStringLiteral("right");
    }
    return QStringLiteral("normal");
}

[[nodiscard]] BusMode busModeFromToken(const QString &token, const BusMode fallback)
{
    if (token == QStringLiteral("normal")) {
        return BusMode::Normal;
    }
    if (token == QStringLiteral("swap")) {
        return BusMode::SwapChannels;
    }
    if (token == QStringLiteral("left")) {
        return BusMode::LeftToBoth;
    }
    if (token == QStringLiteral("right")) {
        return BusMode::RightToBoth;
    }
    return fallback;
}

[[nodiscard]] QVariantMap busProcessingMap(const BusProcessing &p)
{
    return QVariantMap{{QStringLiteral("equalizer"), equalizerMap(p.equalizer)},
                       {QStringLiteral("mode"), busModeToken(p.mode)}};
}

[[nodiscard]] QVariantMap processingMap(const StripProcessing &p)
{
    return QVariantMap{
        {QStringLiteral("denoiser"),
         QVariantMap{{QStringLiteral("enabled"), p.denoiser.enabled},
                     {QStringLiteral("vadThreshold"), p.denoiser.vadThreshold}}},
        {QStringLiteral("gate"),
         QVariantMap{{QStringLiteral("enabled"), p.gate.enabled},
                     {QStringLiteral("thresholdDb"), p.gate.thresholdDb},
                     {QStringLiteral("attackMs"), p.gate.attackMs},
                     {QStringLiteral("holdMs"), p.gate.holdMs},
                     {QStringLiteral("releaseMs"), p.gate.releaseMs},
                     {QStringLiteral("rangeDb"), p.gate.rangeDb}}},
        {QStringLiteral("compressor"),
         QVariantMap{{QStringLiteral("enabled"), p.compressor.enabled},
                     {QStringLiteral("thresholdDb"), p.compressor.thresholdDb},
                     {QStringLiteral("ratio"), p.compressor.ratio},
                     {QStringLiteral("attackMs"), p.compressor.attackMs},
                     {QStringLiteral("releaseMs"), p.compressor.releaseMs},
                     {QStringLiteral("kneeDb"), p.compressor.kneeDb},
                     {QStringLiteral("makeupDb"), p.compressor.makeupDb}}},
        {QStringLiteral("equalizer"),
         QVariantMap{{QStringLiteral("enabled"), p.equalizer.enabled},
                     {QStringLiteral("lowHz"), p.equalizer.lowHz},
                     {QStringLiteral("lowGainDb"), p.equalizer.lowGainDb},
                     {QStringLiteral("midHz"), p.equalizer.midHz},
                     {QStringLiteral("midGainDb"), p.equalizer.midGainDb},
                     {QStringLiteral("midQ"), p.equalizer.midQ},
                     {QStringLiteral("highHz"), p.equalizer.highHz},
                     {QStringLiteral("highGainDb"), p.equalizer.highGainDb}}},
        {QStringLiteral("limiter"),
         QVariantMap{{QStringLiteral("enabled"), p.limiter.enabled},
                     {QStringLiteral("ceilingDb"), p.limiter.ceilingDb},
                     {QStringLiteral("releaseMs"), p.limiter.releaseMs}}}};
}

[[nodiscard]] StripProcessing processingFromMap(const QVariantMap &map,
                                               const StripProcessing &fallback)
{
    const auto block = [&map](const char *name) {
        return map.value(QLatin1String(name)).toMap();
    };
    const auto number = [](const QVariantMap &b, const char *key, const double current) {
        const QVariant value = b.value(QLatin1String(key));
        bool ok = false;
        const double parsed = value.toDouble(&ok);
        return ok ? parsed : current;
    };
    const auto flag = [](const QVariantMap &b, const bool current) {
        const QVariant value = b.value(QStringLiteral("enabled"));
        return value.isValid() ? value.toBool() : current;
    };
    StripProcessing p = fallback;
    const QVariantMap dn = block("denoiser");
    p.denoiser.enabled = flag(dn, p.denoiser.enabled);
    p.denoiser.vadThreshold = number(dn, "vadThreshold", p.denoiser.vadThreshold);
    const QVariantMap gate = block("gate");
    p.gate.enabled = flag(gate, p.gate.enabled);
    p.gate.thresholdDb = number(gate, "thresholdDb", p.gate.thresholdDb);
    p.gate.attackMs = number(gate, "attackMs", p.gate.attackMs);
    p.gate.holdMs = number(gate, "holdMs", p.gate.holdMs);
    p.gate.releaseMs = number(gate, "releaseMs", p.gate.releaseMs);
    p.gate.rangeDb = number(gate, "rangeDb", p.gate.rangeDb);
    const QVariantMap comp = block("compressor");
    p.compressor.enabled = flag(comp, p.compressor.enabled);
    p.compressor.thresholdDb = number(comp, "thresholdDb", p.compressor.thresholdDb);
    p.compressor.ratio = number(comp, "ratio", p.compressor.ratio);
    p.compressor.attackMs = number(comp, "attackMs", p.compressor.attackMs);
    p.compressor.releaseMs = number(comp, "releaseMs", p.compressor.releaseMs);
    p.compressor.kneeDb = number(comp, "kneeDb", p.compressor.kneeDb);
    p.compressor.makeupDb = number(comp, "makeupDb", p.compressor.makeupDb);
    const QVariantMap eq = block("equalizer");
    p.equalizer.enabled = flag(eq, p.equalizer.enabled);
    p.equalizer.lowHz = number(eq, "lowHz", p.equalizer.lowHz);
    p.equalizer.lowGainDb = number(eq, "lowGainDb", p.equalizer.lowGainDb);
    p.equalizer.midHz = number(eq, "midHz", p.equalizer.midHz);
    p.equalizer.midGainDb = number(eq, "midGainDb", p.equalizer.midGainDb);
    p.equalizer.midQ = number(eq, "midQ", p.equalizer.midQ);
    p.equalizer.highHz = number(eq, "highHz", p.equalizer.highHz);
    p.equalizer.highGainDb = number(eq, "highGainDb", p.equalizer.highGainDb);
    const QVariantMap lim = block("limiter");
    p.limiter.enabled = flag(lim, p.limiter.enabled);
    p.limiter.ceilingDb = number(lim, "ceilingDb", p.limiter.ceilingDb);
    p.limiter.releaseMs = number(lim, "releaseMs", p.limiter.releaseMs);
    return p;
}

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
            {QStringLiteral("processing"), processingMap(strip.processing)},
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
            {QStringLiteral("processing"), busProcessingMap(bus.processing)},
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

bool AudioSettingsModel::setStripProcessing(QString stripId, QVariantMap processing)
{
    const Snapshot snapshot = m_client.snapshot();
    if (!consoleAvailable()
        || !snapshot.capabilities.testFlag(Capability::SetConsoleGain)) {
        rejectAction(QStringLiteral("unsupported"));
        return false;
    }
    const Strip *current = nullptr;
    for (const Strip &strip : snapshot.console.strips) {
        if (strip.id == stripId) {
            current = &strip;
        }
    }
    if (current == nullptr) {
        rejectAction(QStringLiteral("stale-handle"));
        return false;
    }
    // Missing keys keep the strip's current values, so a control can send only
    // the block it changed; the service still judges the whole rack.
    const StripProcessing rack = processingFromMap(processing, current->processing);
    if (!validStripProcessing(rack)) {
        rejectAction(QStringLiteral("processing-out-of-range"));
        return false;
    }
    return trackConsoleRequest(m_client.setStripProcessing(stripId, rack));
}

QStringList AudioSettingsModel::consolePresets() const
{
    return m_client.snapshot().console.presets;
}

QStringList AudioSettingsModel::consoleMacros() const
{
    return m_client.snapshot().console.macros;
}

QVariantMap AudioSettingsModel::consoleRecording() const
{
    const Recording recording = m_client.snapshot().console.recording;
    return QVariantMap{{QStringLiteral("active"), recording.active},
                       {QStringLiteral("busId"), recording.busId},
                       {QStringLiteral("path"), recording.path},
                       {QStringLiteral("startedAtMs"), recording.startedAtMs}};
}

QVariantList AudioSettingsModel::consoleVban() const
{
    QVariantList rows;
    for (const VbanStream &stream : m_client.snapshot().console.vban) {
        rows.append(QVariantMap{{QStringLiteral("name"), stream.name},
                                {QStringLiteral("outgoing"), stream.outgoing},
                                {QStringLiteral("busId"), stream.busId},
                                {QStringLiteral("host"), stream.host},
                                {QStringLiteral("port"), stream.port},
                                {QStringLiteral("outputNodeName"), stream.outputNodeName},
                                {QStringLiteral("enabled"), stream.enabled},
                                {QStringLiteral("active"), stream.active}});
    }
    return rows;
}

bool AudioSettingsModel::setVbanEnabled(QString name, const bool enabled)
{
    if (!canManagePeerStreams()) {
        rejectAction(QStringLiteral("unsupported"));
        return false;
    }
    return trackConsoleRequest(m_client.setVbanEnabled(name.trimmed(), enabled));
}

bool AudioSettingsModel::startRecording(QString busId, QString format)
{
    const Snapshot snapshot = m_client.snapshot();
    if (!consoleAvailable() || !snapshot.capabilities.testFlag(Capability::SetConsoleGain)) {
        rejectAction(QStringLiteral("unsupported"));
        return false;
    }
    if (busId.isEmpty()
        || (format != QStringLiteral("flac") && format != QStringLiteral("wav"))) {
        rejectAction(QStringLiteral("unknown-format"));
        return false;
    }
    return trackConsoleRequest(m_client.startRecording(busId, format));
}

bool AudioSettingsModel::stopRecording()
{
    if (!consoleAvailable()) {
        rejectAction(QStringLiteral("unsupported"));
        return false;
    }
    return trackConsoleRequest(m_client.stopRecording());
}

bool AudioSettingsModel::runMacro(QString name)
{
    return dispatchPreset(OperationKind::RunMacro, std::move(name));
}

bool AudioSettingsModel::savePreset(QString name)
{
    return dispatchPreset(OperationKind::SavePreset, std::move(name));
}

bool AudioSettingsModel::loadPreset(QString name)
{
    return dispatchPreset(OperationKind::LoadPreset, std::move(name));
}

bool AudioSettingsModel::deletePreset(QString name)
{
    return dispatchPreset(OperationKind::DeletePreset, std::move(name));
}

bool AudioSettingsModel::dispatchPreset(const OperationKind kind, QString name)
{
    const Snapshot snapshot = m_client.snapshot();
    if (!consoleAvailable() || !snapshot.capabilities.testFlag(Capability::SetConsoleGain)) {
        rejectAction(QStringLiteral("unsupported"));
        return false;
    }
    name = name.trimmed();
    if (name.isEmpty()) {
        rejectAction(QStringLiteral("invalid-preset-name"));
        return false;
    }
    quint64 requestId = 0;
    switch (kind) {
    case OperationKind::SavePreset:
        requestId = m_client.savePreset(name);
        break;
    case OperationKind::LoadPreset:
        requestId = m_client.loadPreset(name);
        break;
    case OperationKind::DeletePreset:
        requestId = m_client.deletePreset(name);
        break;
    case OperationKind::RunMacro:
        requestId = m_client.runMacro(name);
        break;
    default:
        break;
    }
    return trackConsoleRequest(requestId);
}

bool AudioSettingsModel::setBusProcessing(QString busId, QVariantMap processing)
{
    const Snapshot snapshot = m_client.snapshot();
    if (!consoleAvailable()
        || !snapshot.capabilities.testFlag(Capability::SetConsoleGain)) {
        rejectAction(QStringLiteral("unsupported"));
        return false;
    }
    const Bus *current = nullptr;
    for (const Bus &bus : snapshot.console.buses) {
        if (bus.id == busId) {
            current = &bus;
        }
    }
    if (current == nullptr) {
        rejectAction(QStringLiteral("stale-handle"));
        return false;
    }
    BusProcessing rack = current->processing;
    const QVariantMap eq = processing.value(QStringLiteral("equalizer")).toMap();
    const auto number = [&eq](const char *key, const double fallback) {
        bool ok = false;
        const double parsed = eq.value(QLatin1String(key)).toDouble(&ok);
        return ok ? parsed : fallback;
    };
    if (eq.contains(QStringLiteral("enabled"))) {
        rack.equalizer.enabled = eq.value(QStringLiteral("enabled")).toBool();
    }
    rack.equalizer.lowHz = number("lowHz", rack.equalizer.lowHz);
    rack.equalizer.lowGainDb = number("lowGainDb", rack.equalizer.lowGainDb);
    rack.equalizer.midHz = number("midHz", rack.equalizer.midHz);
    rack.equalizer.midGainDb = number("midGainDb", rack.equalizer.midGainDb);
    rack.equalizer.midQ = number("midQ", rack.equalizer.midQ);
    rack.equalizer.highHz = number("highHz", rack.equalizer.highHz);
    rack.equalizer.highGainDb = number("highGainDb", rack.equalizer.highGainDb);
    rack.mode = busModeFromToken(processing.value(QStringLiteral("mode")).toString(), rack.mode);
    if (!validBusProcessing(rack)) {
        rejectAction(QStringLiteral("processing-out-of-range"));
        return false;
    }
    return trackConsoleRequest(m_client.setBusProcessing(busId, rack));
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
    return trackConsoleRequest(requestId);
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
    return trackConsoleRequest(requestId);
}

bool AudioSettingsModel::trackConsoleRequest(const quint64 requestId)
{
    if (requestId == 0) {
        rejectAction(QString());
        return false;
    }
    // AudioClient returns an id even for a queued local refusal (including
    // Busy). Console ids have no graph serial, but their replies need exactly
    // the same failure/uncertainty feedback as device and stream operations.
    m_consoleRequestIds.insert(requestId);
    m_localError.clear();
    m_operationStatusText = tr("Applying the console change…");
    Q_EMIT viewChanged();
    return true;
}

} // namespace QindaQt::Apps::SettingsAudio
