// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/audio_service/console_model.h>

#include <qindaqt/services/audio_protocol/audio_gain.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtCore/QJsonArray>

#include <algorithm>
#include <cmath>

namespace QindaQt::Audio
{
namespace {

QJsonObject processingToJson(const StripProcessing &p)
{
    return QJsonObject{
        {QStringLiteral("gate"),
         QJsonObject{{QStringLiteral("on"), p.gate.enabled},
                     {QStringLiteral("thresholdDb"), p.gate.thresholdDb},
                     {QStringLiteral("attackMs"), p.gate.attackMs},
                     {QStringLiteral("holdMs"), p.gate.holdMs},
                     {QStringLiteral("releaseMs"), p.gate.releaseMs},
                     {QStringLiteral("rangeDb"), p.gate.rangeDb}}},
        {QStringLiteral("compressor"),
         QJsonObject{{QStringLiteral("on"), p.compressor.enabled},
                     {QStringLiteral("thresholdDb"), p.compressor.thresholdDb},
                     {QStringLiteral("ratio"), p.compressor.ratio},
                     {QStringLiteral("attackMs"), p.compressor.attackMs},
                     {QStringLiteral("releaseMs"), p.compressor.releaseMs},
                     {QStringLiteral("kneeDb"), p.compressor.kneeDb},
                     {QStringLiteral("makeupDb"), p.compressor.makeupDb}}},
        {QStringLiteral("equalizer"),
         QJsonObject{{QStringLiteral("on"), p.equalizer.enabled},
                     {QStringLiteral("lowHz"), p.equalizer.lowHz},
                     {QStringLiteral("lowGainDb"), p.equalizer.lowGainDb},
                     {QStringLiteral("midHz"), p.equalizer.midHz},
                     {QStringLiteral("midGainDb"), p.equalizer.midGainDb},
                     {QStringLiteral("midQ"), p.equalizer.midQ},
                     {QStringLiteral("highHz"), p.equalizer.highHz},
                     {QStringLiteral("highGainDb"), p.equalizer.highGainDb}}},
        {QStringLiteral("limiter"),
         QJsonObject{{QStringLiteral("on"), p.limiter.enabled},
                     {QStringLiteral("ceilingDb"), p.limiter.ceilingDb},
                     {QStringLiteral("releaseMs"), p.limiter.releaseMs}}}};
}

StripProcessing processingFromJson(const QJsonObject &object, const StripProcessing &fallback)
{
    if (object.isEmpty()) {
        return fallback;
    }
    const auto number = [](const QJsonObject &block, const char *key, const double current) {
        return block.value(QLatin1String(key)).toDouble(current);
    };
    StripProcessing p = fallback;
    const QJsonObject gate = object.value(QStringLiteral("gate")).toObject();
    p.gate.enabled = gate.value(QStringLiteral("on")).toBool(p.gate.enabled);
    p.gate.thresholdDb = number(gate, "thresholdDb", p.gate.thresholdDb);
    p.gate.attackMs = number(gate, "attackMs", p.gate.attackMs);
    p.gate.holdMs = number(gate, "holdMs", p.gate.holdMs);
    p.gate.releaseMs = number(gate, "releaseMs", p.gate.releaseMs);
    p.gate.rangeDb = number(gate, "rangeDb", p.gate.rangeDb);
    const QJsonObject comp = object.value(QStringLiteral("compressor")).toObject();
    p.compressor.enabled = comp.value(QStringLiteral("on")).toBool(p.compressor.enabled);
    p.compressor.thresholdDb = number(comp, "thresholdDb", p.compressor.thresholdDb);
    p.compressor.ratio = number(comp, "ratio", p.compressor.ratio);
    p.compressor.attackMs = number(comp, "attackMs", p.compressor.attackMs);
    p.compressor.releaseMs = number(comp, "releaseMs", p.compressor.releaseMs);
    p.compressor.kneeDb = number(comp, "kneeDb", p.compressor.kneeDb);
    p.compressor.makeupDb = number(comp, "makeupDb", p.compressor.makeupDb);
    const QJsonObject eq = object.value(QStringLiteral("equalizer")).toObject();
    p.equalizer.enabled = eq.value(QStringLiteral("on")).toBool(p.equalizer.enabled);
    p.equalizer.lowHz = number(eq, "lowHz", p.equalizer.lowHz);
    p.equalizer.lowGainDb = number(eq, "lowGainDb", p.equalizer.lowGainDb);
    p.equalizer.midHz = number(eq, "midHz", p.equalizer.midHz);
    p.equalizer.midGainDb = number(eq, "midGainDb", p.equalizer.midGainDb);
    p.equalizer.midQ = number(eq, "midQ", p.equalizer.midQ);
    p.equalizer.highHz = number(eq, "highHz", p.equalizer.highHz);
    p.equalizer.highGainDb = number(eq, "highGainDb", p.equalizer.highGainDb);
    const QJsonObject lim = object.value(QStringLiteral("limiter")).toObject();
    p.limiter.enabled = lim.value(QStringLiteral("on")).toBool(p.limiter.enabled);
    p.limiter.ceilingDb = number(lim, "ceilingDb", p.limiter.ceilingDb);
    p.limiter.releaseMs = number(lim, "releaseMs", p.limiter.releaseMs);
    return p;
}

[[nodiscard]] bool reject(QString *reasonCode, const char *code)
{
    if (reasonCode != nullptr) {
        *reasonCode = QString::fromLatin1(code);
    }
    return false;
}

[[nodiscard]] bool finiteGain(double value)
{
    return std::isfinite(value) && value >= kMinGainDb && value <= kMaxGainDb;
}

} // namespace

ConsoleModel::ConsoleModel()
{
    // AGENT-NOTE: the default layout is the reference console's, so a fresh
    // install already looks like the product users are comparing against
    // rather than an empty rack they must assemble first.
    for (int index = 0; index < kHardwareStrips; ++index) {
        Strip strip;
        strip.id = QStringLiteral("strip.hw.%1").arg(index + 1);
        strip.kind = StripKind::HardwareInput;
        strip.index = static_cast<quint32>(index);
        strip.label = QStringLiteral("Hardware Input %1").arg(index + 1);
        m_strips.append(std::move(strip));
    }
    for (int index = 0; index < kVirtualStrips; ++index) {
        Strip strip;
        strip.id = QStringLiteral("strip.virtual.%1").arg(index + 1);
        strip.kind = StripKind::VirtualInput;
        strip.index = static_cast<quint32>(kHardwareStrips + index);
        strip.label = index == 0 ? QStringLiteral("Virtual Input")
                                 : QStringLiteral("Virtual Input %1").arg(index + 1);
        m_strips.append(std::move(strip));
    }
    for (int index = 0; index < kPhysicalBuses; ++index) {
        Bus bus;
        bus.id = QStringLiteral("bus.a%1").arg(index + 1);
        bus.kind = BusKind::Physical;
        bus.index = static_cast<quint32>(index);
        bus.label = QStringLiteral("A%1").arg(index + 1);
        m_buses.append(std::move(bus));
    }
    for (int index = 0; index < kVirtualBuses; ++index) {
        Bus bus;
        bus.id = QStringLiteral("bus.b%1").arg(index + 1);
        bus.kind = BusKind::Virtual;
        bus.index = static_cast<quint32>(kPhysicalBuses + index);
        bus.label = QStringLiteral("B%1").arg(index + 1);
        m_buses.append(std::move(bus));
    }
    // Every strip carries a send for every bus, so the matrix is rectangular
    // and a client can index it without asking which cells exist.
    for (Strip &strip : m_strips) {
        strip.sends.reserve(m_buses.size());
        for (const Bus &bus : m_buses) {
            strip.sends.append(MatrixSend{bus.index, false, 0.0});
        }
    }
}

Console ConsoleModel::console() const
{
    Console console;
    console.strips = m_strips;
    console.buses = m_buses;
    console.soloActive = std::any_of(m_strips.cbegin(), m_strips.cend(),
                                     [](const Strip &strip) { return strip.soloed; });
    return console;
}

Strip *ConsoleModel::findStrip(const QString &id)
{
    const auto it = std::find_if(m_strips.begin(), m_strips.end(),
                                 [&id](const Strip &strip) { return strip.id == id; });
    return it == m_strips.end() ? nullptr : &*it;
}

Bus *ConsoleModel::findBus(const QString &id)
{
    const auto it = std::find_if(m_buses.begin(), m_buses.end(),
                                 [&id](const Bus &bus) { return bus.id == id; });
    return it == m_buses.end() ? nullptr : &*it;
}

const Bus *ConsoleModel::findBusByIndex(quint32 index) const
{
    const auto it = std::find_if(m_buses.cbegin(), m_buses.cend(),
                                 [index](const Bus &bus) { return bus.index == index; });
    return it == m_buses.cend() ? nullptr : &*it;
}

bool ConsoleModel::apply(const OperationRequest &request, QString *reasonCode)
{
    switch (request.kind) {
    case OperationKind::SetStripGain:
    case OperationKind::SetStripMute:
    case OperationKind::SetStripSolo:
    case OperationKind::SetStripMono:
    case OperationKind::SetStripPan:
    case OperationKind::SetStripTrim:
    case OperationKind::SetStripSend:
    case OperationKind::SetStripSource:
    case OperationKind::SetStripProcessing: {
        Strip *const strip = findStrip(request.consoleId);
        if (strip == nullptr) {
            return reject(reasonCode, "unknown-strip");
        }
        switch (request.kind) {
        case OperationKind::SetStripGain:
            if (!finiteGain(request.gainDb)) {
                return reject(reasonCode, "gain-out-of-range");
            }
            strip->gainDb = request.gainDb;
            return true;
        case OperationKind::SetStripMute:
            strip->muted = request.muted;
            return true;
        case OperationKind::SetStripSolo:
            strip->soloed = request.enabled;
            return true;
        case OperationKind::SetStripMono:
            strip->mono = request.enabled;
            return true;
        case OperationKind::SetStripPan:
            if (!std::isfinite(request.pan) || request.pan < kMinPan
                || request.pan > kMaxPan) {
                return reject(reasonCode, "pan-out-of-range");
            }
            strip->pan = request.pan;
            return true;
        case OperationKind::SetStripTrim: {
            if (request.channelVolumes.size() > kMaxChannelsPerDevice) {
                return reject(reasonCode, "too-many-channels");
            }
            for (const double trim : request.channelVolumes) {
                if (!finiteGain(trim)) {
                    return reject(reasonCode, "gain-out-of-range");
                }
            }
            strip->channelTrimDb = request.channelVolumes;
            return true;
        }
        case OperationKind::SetStripProcessing:
            // The whole rack at once (ADR-0179), refused whole when any value
            // is out of range: a half-applied rack would leave the user's
            // compressor at one setting and their gate at another.
            if (!validStripProcessing(request.processing)) {
                return reject(reasonCode, "processing-out-of-range");
            }
            strip->processing = request.processing;
            return true;
        case OperationKind::SetStripSource:
            // AGENT-CONTRACT: the pin is the device's NAME, resolved by the
            // coordinator from the handle the client sent. Empty clears it.
            // The binding itself is not touched here: the coordinator rebinds
            // against the graph on the next publication, which is also what
            // makes a pin to an absent device leave the strip unbound.
            if (!isBoundedText(request.nodeName, kMaxNodeNameUtf8Bytes)) {
                return reject(reasonCode, "invalid-device-name");
            }
            strip->pinnedSource = request.nodeName;
            return true;
        case OperationKind::SetStripSend: {
            if (findBusByIndex(request.busIndex) == nullptr) {
                return reject(reasonCode, "unknown-bus");
            }
            if (!finiteGain(request.gainDb)) {
                return reject(reasonCode, "gain-out-of-range");
            }
            for (MatrixSend &send : strip->sends) {
                if (send.busIndex == request.busIndex) {
                    // AGENT-GUARD: enabled and gainDb move together here on
                    // purpose, but a client that only means to toggle sends the
                    // gain it already read back, so turning a send off and on
                    // again never silently resets it to unity.
                    send.enabled = request.enabled;
                    send.gainDb = request.gainDb;
                    return true;
                }
            }
            return reject(reasonCode, "unknown-bus");
        }
        default:
            break;
        }
        return reject(reasonCode, "unsupported-operation");
    }
    case OperationKind::SetBusGain:
    case OperationKind::SetBusMute:
    case OperationKind::SetBusMono:
    case OperationKind::SetBusTarget: {
        Bus *const bus = findBus(request.consoleId);
        if (bus == nullptr) {
            return reject(reasonCode, "unknown-bus");
        }
        switch (request.kind) {
        case OperationKind::SetBusGain:
            if (!finiteGain(request.gainDb)) {
                return reject(reasonCode, "gain-out-of-range");
            }
            bus->gainDb = request.gainDb;
            return true;
        case OperationKind::SetBusMute:
            bus->muted = request.muted;
            return true;
        case OperationKind::SetBusMono:
            bus->mono = request.enabled;
            return true;
        case OperationKind::SetBusTarget:
            if (!isBoundedText(request.nodeName, kMaxNodeNameUtf8Bytes)) {
                return reject(reasonCode, "invalid-device-name");
            }
            // A pin, not a binding (ADR-0178): the handle the client sent is
            // resolved to a name by the coordinator, and the graph binds it.
            // Writing the handle here, as this once did, was overwritten by
            // automatic binding on the very next publication.
            bus->pinnedTarget = request.nodeName;
            return true;
        default:
            break;
        }
        return reject(reasonCode, "unsupported-operation");
    }
    default:
        break;
    }
    return reject(reasonCode, "unsupported-operation");
}

void ConsoleModel::bindStripSource(const QString &stripId, Handle source, bool known)
{
    if (Strip *const strip = findStrip(stripId); strip != nullptr) {
        strip->sourceEpoch = source.epoch;
        strip->sourceSerial = source.serial;
        strip->sourceKnown = known && source.isValid();
    }
}

void ConsoleModel::bindBusTarget(const QString &busId, Handle target, bool known)
{
    if (Bus *const bus = findBus(busId); bus != nullptr) {
        bus->targetEpoch = target.epoch;
        bus->targetSerial = target.serial;
        bus->targetKnown = known && target.isValid();
    }
}

void ConsoleModel::publishStripLevel(const QString &stripId, Level level)
{
    if (level.known
        && (!std::isfinite(level.peakDb) || !std::isfinite(level.rmsDb)
            || level.peakDb > kMaxMeterDb || level.peakDb < kSilentMeterDb
            || level.rmsDb > level.peakDb || level.rmsDb < kSilentMeterDb)) {
        return;
    }
    if (Strip *const strip = findStrip(stripId); strip != nullptr) {
        strip->level = level;
    }
}

bool ConsoleModel::publishLevels(const QList<LevelReading> &levels)
{
    bool changed = false;
    if (levels.isEmpty()) {
        for (Strip &strip : m_strips) {
            changed = changed || strip.level != Level{};
            strip.level = Level{};
        }
        for (Bus &bus : m_buses) {
            changed = changed || bus.level != Level{};
            bus.level = Level{};
        }
        return changed;
    }
    for (const LevelReading &reading : levels) {
        // A reading that names nothing this console publishes is dropped: a
        // meter must never bring an element into existence.
        if (Strip *const strip = findStrip(reading.id); strip != nullptr) {
            const Level before = strip->level;
            publishStripLevel(reading.id, reading.level);
            changed = changed || strip->level != before;
            continue;
        }
        if (Bus *const bus = findBus(reading.id); bus != nullptr) {
            const Level before = bus->level;
            publishBusLevel(reading.id, reading.level);
            changed = changed || bus->level != before;
        }
    }
    return changed;
}

void ConsoleModel::publishBusLevel(const QString &busId, Level level)
{
    if (level.known
        && (!std::isfinite(level.peakDb) || !std::isfinite(level.rmsDb)
            || level.peakDb > kMaxMeterDb || level.peakDb < kSilentMeterDb
            || level.rmsDb > level.peakDb || level.rmsDb < kSilentMeterDb)) {
        return;
    }
    if (Bus *const bus = findBus(busId); bus != nullptr) {
        bus->level = level;
    }
}

QList<ConsoleModel::RoutingEdge> ConsoleModel::routing() const
{
    const bool soloActive = std::any_of(m_strips.cbegin(), m_strips.cend(),
                                        [](const Strip &strip) { return strip.soloed; });
    QList<RoutingEdge> edges;
    for (const Strip &strip : m_strips) {
        const bool audible = !strip.muted && (!soloActive || strip.soloed);
        for (const MatrixSend &send : strip.sends) {
            if (!send.enabled) {
                continue;
            }
            const Bus *const bus = findBusByIndex(send.busIndex);
            if (bus == nullptr) {
                continue;
            }
            edges.append(RoutingEdge{strip.id, bus->id, send.busIndex, send.gainDb,
                                     audible && !bus->muted, strip.pan});
        }
    }
    return edges;
}

QJsonObject ConsoleModel::toJson() const
{
    QJsonArray strips;
    for (const Strip &strip : m_strips) {
        QJsonArray sends;
        for (const MatrixSend &send : strip.sends) {
            sends.append(QJsonObject{{QStringLiteral("bus"), int(send.busIndex)},
                                     {QStringLiteral("on"), send.enabled},
                                     {QStringLiteral("gainDb"), send.gainDb}});
        }
        QJsonArray trims;
        for (const double trim : strip.channelTrimDb) {
            trims.append(trim);
        }
        strips.append(QJsonObject{{QStringLiteral("id"), strip.id},
                                  {QStringLiteral("label"), strip.label},
                                  {QStringLiteral("gainDb"), strip.gainDb},
                                  {QStringLiteral("muted"), strip.muted},
                                  {QStringLiteral("soloed"), strip.soloed},
                                  {QStringLiteral("mono"), strip.mono},
                                  {QStringLiteral("pan"), strip.pan},
                                  {QStringLiteral("trimDb"), trims},
                                  {QStringLiteral("sends"), sends},
                                  {QStringLiteral("source"), strip.pinnedSource},
                                  {QStringLiteral("processing"),
                                   processingToJson(strip.processing)}});
    }
    QJsonArray buses;
    for (const Bus &bus : m_buses) {
        buses.append(QJsonObject{{QStringLiteral("id"), bus.id},
                                 {QStringLiteral("label"), bus.label},
                                 {QStringLiteral("gainDb"), bus.gainDb},
                                 {QStringLiteral("muted"), bus.muted},
                                 {QStringLiteral("mono"), bus.mono},
                                 {QStringLiteral("target"), bus.pinnedTarget}});
    }
    return QJsonObject{{QStringLiteral("schemaVersion"), int(kSchemaVersion)},
                       {QStringLiteral("strips"), strips},
                       {QStringLiteral("buses"), buses}};
}

void ConsoleModel::loadJson(const QJsonObject &document)
{
    const auto readGain = [](const QJsonValue &value, double fallback) {
        const double candidate = value.toDouble(fallback);
        return finiteGain(candidate) ? candidate : fallback;
    };
    for (const QJsonValue &entry : document.value(QStringLiteral("strips")).toArray()) {
        const QJsonObject object = entry.toObject();
        Strip *const strip = findStrip(object.value(QStringLiteral("id")).toString());
        if (strip == nullptr) {
            continue;
        }
        const QString label = object.value(QStringLiteral("label")).toString();
        if (isBoundedText(label, kMaxConsoleLabelUtf8Bytes) && !label.isEmpty()) {
            strip->label = label;
        }
        strip->gainDb = readGain(object.value(QStringLiteral("gainDb")), strip->gainDb);
        strip->muted = object.value(QStringLiteral("muted")).toBool(strip->muted);
        strip->soloed = object.value(QStringLiteral("soloed")).toBool(strip->soloed);
        strip->mono = object.value(QStringLiteral("mono")).toBool(strip->mono);
        const QString source = object.value(QStringLiteral("source")).toString();
        if (isBoundedText(source, kMaxNodeNameUtf8Bytes)) {
            strip->pinnedSource = source;
        }
        // A rack that fails the bounds is left at the strip's current rack
        // rather than partially applied.
        const StripProcessing processing = processingFromJson(
            object.value(QStringLiteral("processing")).toObject(), strip->processing);
        if (validStripProcessing(processing)) {
            strip->processing = processing;
        }
        const double pan = object.value(QStringLiteral("pan")).toDouble(strip->pan);
        if (std::isfinite(pan) && pan >= kMinPan && pan <= kMaxPan) {
            strip->pan = pan;
        }
        QVector<double> trims;
        for (const QJsonValue &trim : object.value(QStringLiteral("trimDb")).toArray()) {
            if (trims.size() >= kMaxChannelsPerDevice) {
                break;
            }
            const double value = trim.toDouble(0.0);
            trims.append(finiteGain(value) ? value : 0.0);
        }
        strip->channelTrimDb = trims;
        for (const QJsonValue &sendValue :
             object.value(QStringLiteral("sends")).toArray()) {
            const QJsonObject sendObject = sendValue.toObject();
            const auto busIndex =
                static_cast<quint32>(sendObject.value(QStringLiteral("bus")).toInt(-1));
            for (MatrixSend &send : strip->sends) {
                if (send.busIndex != busIndex) {
                    continue;
                }
                send.enabled = sendObject.value(QStringLiteral("on")).toBool(false);
                send.gainDb = readGain(sendObject.value(QStringLiteral("gainDb")), 0.0);
            }
        }
    }
    for (const QJsonValue &entry : document.value(QStringLiteral("buses")).toArray()) {
        const QJsonObject object = entry.toObject();
        Bus *const bus = findBus(object.value(QStringLiteral("id")).toString());
        if (bus == nullptr) {
            continue;
        }
        const QString label = object.value(QStringLiteral("label")).toString();
        if (isBoundedText(label, kMaxConsoleLabelUtf8Bytes) && !label.isEmpty()) {
            bus->label = label;
        }
        bus->gainDb = readGain(object.value(QStringLiteral("gainDb")), bus->gainDb);
        bus->muted = object.value(QStringLiteral("muted")).toBool(bus->muted);
        bus->mono = object.value(QStringLiteral("mono")).toBool(bus->mono);
        const QString target = object.value(QStringLiteral("target")).toString();
        if (isBoundedText(target, kMaxNodeNameUtf8Bytes)) {
            bus->pinnedTarget = target;
        }
    }
}

} // namespace QindaQt::Audio
