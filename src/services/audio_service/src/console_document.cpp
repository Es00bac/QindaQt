// SPDX-License-Identifier: LGPL-3.0-or-later
// Console JSON codec only. Runtime mutations/bindings remain in ConsoleModel;
// this extraction preserves the document schema and bounded legacy loading.
#include <qindaqt/services/audio_service/console_model.h>
#include <qindaqt/services/audio_protocol/audio_gain.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>
#include <QtCore/QJsonArray>
#include <cmath>

namespace QindaQt::Audio
{
namespace {
[[nodiscard]] bool finiteGain(const double value)
{
    return std::isfinite(value) && value >= kMinGainDb && value <= kMaxGainDb;
}
QJsonObject processingToJson(const StripProcessing &p)
{
    return QJsonObject{
        {QStringLiteral("denoiser"),
         QJsonObject{{QStringLiteral("on"), p.denoiser.enabled},
                     {QStringLiteral("vadThreshold"), p.denoiser.vadThreshold}}},
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

QJsonObject busProcessingToJson(const BusProcessing &p)
{
    return QJsonObject{
        {QStringLiteral("equalizer"),
         QJsonObject{{QStringLiteral("on"), p.equalizer.enabled},
                     {QStringLiteral("lowHz"), p.equalizer.lowHz},
                     {QStringLiteral("lowGainDb"), p.equalizer.lowGainDb},
                     {QStringLiteral("midHz"), p.equalizer.midHz},
                     {QStringLiteral("midGainDb"), p.equalizer.midGainDb},
                     {QStringLiteral("midQ"), p.equalizer.midQ},
                     {QStringLiteral("highHz"), p.equalizer.highHz},
                     {QStringLiteral("highGainDb"), p.equalizer.highGainDb}}},
        {QStringLiteral("mode"), int(p.mode)},
        {QStringLiteral("delayMs"), p.delayMs}};
}

BusProcessing busProcessingFromJson(const QJsonObject &object, const BusProcessing &fallback)
{
    if (object.isEmpty()) {
        return fallback;
    }
    const auto number = [](const QJsonObject &block, const char *key, const double current) {
        return block.value(QLatin1String(key)).toDouble(current);
    };
    BusProcessing p = fallback;
    const QJsonObject eq = object.value(QStringLiteral("equalizer")).toObject();
    p.equalizer.enabled = eq.value(QStringLiteral("on")).toBool(p.equalizer.enabled);
    p.equalizer.lowHz = number(eq, "lowHz", p.equalizer.lowHz);
    p.equalizer.lowGainDb = number(eq, "lowGainDb", p.equalizer.lowGainDb);
    p.equalizer.midHz = number(eq, "midHz", p.equalizer.midHz);
    p.equalizer.midGainDb = number(eq, "midGainDb", p.equalizer.midGainDb);
    p.equalizer.midQ = number(eq, "midQ", p.equalizer.midQ);
    p.equalizer.highHz = number(eq, "highHz", p.equalizer.highHz);
    p.equalizer.highGainDb = number(eq, "highGainDb", p.equalizer.highGainDb);
    p.mode = static_cast<BusMode>(object.value(QStringLiteral("mode")).toInt(int(p.mode)));
    p.delayMs = object.value(QStringLiteral("delayMs")).toInt(p.delayMs);
    return p;
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
    const QJsonObject dn = object.value(QStringLiteral("denoiser")).toObject();
    p.denoiser.enabled = dn.value(QStringLiteral("on")).toBool(p.denoiser.enabled);
    p.denoiser.vadThreshold = number(dn, "vadThreshold", p.denoiser.vadThreshold);
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

} // namespace

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
                                 {QStringLiteral("target"), bus.pinnedTarget},
                                 {QStringLiteral("processing"),
                                  busProcessingToJson(bus.processing)}});
    }
    return QJsonObject{{QStringLiteral("schemaVersion"), int(kSchemaVersion)},
                       {QStringLiteral("strips"), strips},
                       {QStringLiteral("buses"), buses},
                       {QStringLiteral("vbanEnabled"), QJsonArray::fromStringList(m_enabledVban)}};
}

void ConsoleModel::loadJson(const QJsonObject &document)
{
    m_enabledVban.clear();
    for (const QJsonValue &value : document.value(QStringLiteral("vbanEnabled")).toArray()) {
        setVbanEnabled(value.toString(), true);
    }
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
        const BusProcessing busRack = busProcessingFromJson(
            object.value(QStringLiteral("processing")).toObject(), bus->processing);
        if (validBusProcessing(busRack)) {
            bus->processing = busRack;
        }
    }
}

} // namespace QindaQt::Audio
