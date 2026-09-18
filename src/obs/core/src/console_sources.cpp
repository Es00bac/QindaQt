// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/obs_bridge/console_sources.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>

namespace QindaQt::ObsBridge {
namespace {

constexpr char kConsoleNodeNamePrefix[] = "qindaqt.console.";

QString idTail(const QString &consoleId, const QString &head)
{
    return consoleId.startsWith(head) ? consoleId.mid(head.size()) : consoleId;
}

QString titleWords(const QString &tail)
{
    QStringList words = tail.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    for (QString &word : words) {
        if (!word.isEmpty()) {
            word[0] = word.at(0).toUpper();
        }
    }
    return words.join(QLatin1Char(' '));
}

std::optional<Audio::Device> deviceFor(const QList<Audio::Device> &devices,
                                       quint64 epoch, quint64 serial)
{
    for (const Audio::Device &device : devices) {
        if (device.handle.epoch == epoch && device.handle.serial == serial) {
            return device;
        }
    }
    return std::nullopt;
}

} // namespace

QString sourceKindId(SourceKind kind)
{
    return QString::fromLatin1(kind == SourceKind::Bus ? BusSourceId : StripSourceId);
}

std::optional<SourceKind> sourceKindFromId(const QString &id)
{
    if (id == QLatin1String(BusSourceId)) {
        return SourceKind::Bus;
    }
    if (id == QLatin1String(StripSourceId)) {
        return SourceKind::Strip;
    }
    return std::nullopt;
}

QString captureKindToken(CaptureKind kind)
{
    switch (kind) {
    case CaptureKind::Input:
        return QStringLiteral("input");
    case CaptureKind::Monitor:
        return QStringLiteral("monitor");
    case CaptureKind::None:
        break;
    }
    return QStringLiteral("none");
}

std::optional<CaptureKind> captureKindFromToken(const QString &token)
{
    if (token == QLatin1String("input")) {
        return CaptureKind::Input;
    }
    if (token == QLatin1String("monitor")) {
        return CaptureKind::Monitor;
    }
    if (token == QLatin1String("none")) {
        return CaptureKind::None;
    }
    return std::nullopt;
}

QString busCode(const Audio::Bus &bus)
{
    static const QRegularExpression shortCode(QStringLiteral("^[ab][0-9]{1,2}$"));
    const QString tail = idTail(bus.id, QStringLiteral("bus."));
    if (shortCode.match(tail).hasMatch()) {
        return tail.toUpper();
    }
    return QString(bus.kind == Audio::BusKind::Physical ? QLatin1Char('A') : QLatin1Char('B'))
        + QString::number(bus.index + 1);
}

QString stripCode(const Audio::Strip &strip)
{
    const QString tail = idTail(strip.id, QStringLiteral("strip."));
    const QString words = titleWords(tail);
    if (!words.isEmpty()) {
        return words;
    }
    return QString(strip.kind == Audio::StripKind::HardwareInput ? QStringLiteral("Hardware ")
                                                                  : QStringLiteral("Virtual "))
        + QString::number(strip.index + 1);
}

QString sourceNameFor(SourceKind kind, const QString &code, const QString &label)
{
    QString name = QStringLiteral("QindaQt ")
        + (kind == SourceKind::Bus ? QStringLiteral("Bus ") : QStringLiteral("Strip ")) + code;
    const QString trimmed = label.trimmed();
    if (!trimmed.isEmpty() && trimmed.compare(code, Qt::CaseInsensitive) != 0) {
        name += QStringLiteral(" — ") + trimmed;
    }
    return name;
}

QString consoleNodeName(const QString &consoleId)
{
    return QLatin1String(kConsoleNodeNamePrefix) + consoleId;
}

QList<DesiredSource> desiredSources(const Audio::Snapshot &snapshot)
{
    QList<DesiredSource> sources;
    const Audio::Console &console = snapshot.console;
    for (const Audio::Bus &bus : console.buses) {
        if (bus.id.isEmpty()) {
            continue;
        }
        DesiredSource source;
        source.consoleId = bus.id;
        source.kind = SourceKind::Bus;
        source.code = busCode(bus);
        source.label = bus.label;
        source.muted = bus.muted;
        source.gainDb = bus.gainDb;
        if (bus.kind == Audio::BusKind::Virtual) {
            // Applications record a virtual bus from its `.source`; OBS is one.
            source.captureKind = CaptureKind::Input;
            source.captureDevice = consoleNodeName(bus.id) + QStringLiteral(".source");
        } else if (bus.targetKnown) {
            // A physical bus plays into a real device: capture that device's
            // monitor, so OBS hears exactly what the speakers get.
            if (const auto target = deviceFor(snapshot.outputs, bus.targetEpoch, bus.targetSerial);
                target && !target->nodeName.isEmpty()) {
                source.captureKind = CaptureKind::Monitor;
                source.captureDevice = target->nodeName + QStringLiteral(".monitor");
            }
        }
        sources.append(source);
    }
    for (const Audio::Strip &strip : console.strips) {
        if (strip.id.isEmpty()) {
            continue;
        }
        DesiredSource source;
        source.consoleId = strip.id;
        source.kind = SourceKind::Strip;
        source.code = stripCode(strip);
        source.label = strip.label;
        source.muted = strip.muted;
        source.gainDb = strip.gainDb;
        if (strip.kind == Audio::StripKind::VirtualInput) {
            // The strip's sink carries what the application plays into it.
            source.captureKind = CaptureKind::Monitor;
            source.captureDevice = consoleNodeName(strip.id) + QStringLiteral(".monitor");
        } else if (strip.sourceKnown) {
            if (const auto device = deviceFor(snapshot.inputs, strip.sourceEpoch, strip.sourceSerial);
                device && !device->nodeName.isEmpty()) {
                source.captureKind = CaptureKind::Input;
                source.captureDevice = device->nodeName;
            }
        }
        sources.append(source);
    }
    // Names are unique: a second source that would take an already used name
    // carries its console id, so two "Speakers" buses stay distinguishable.
    QSet<QString> used;
    for (DesiredSource &source : sources) {
        QString name = sourceNameFor(source.kind, source.code, source.label);
        if (used.contains(name)) {
            name += QStringLiteral(" [") + source.consoleId + QLatin1Char(']');
        }
        used.insert(name);
        source.sourceName = name;
    }
    return sources;
}

SyncPlan planSync(const QList<DesiredSource> &desired, const QList<ExistingSource> &existing)
{
    SyncPlan plan;
    QHash<QString, ExistingSource> byId;
    for (const ExistingSource &source : existing) {
        // A duplicate console id (two sources claiming one bus) is not
        // reconcilable by renaming: the later one goes.
        if (byId.contains(source.consoleId)) {
            plan.removals.append(source.consoleId + QStringLiteral("#") + source.sourceName);
            continue;
        }
        byId.insert(source.consoleId, source);
    }
    QSet<QString> desiredIds;
    for (const DesiredSource &source : desired) {
        desiredIds.insert(source.consoleId);
    }
    for (const ExistingSource &source : existing) {
        if (!desiredIds.contains(source.consoleId) && byId.value(source.consoleId) == source) {
            plan.removals.append(source.consoleId);
        }
    }
    for (const DesiredSource &source : desired) {
        const auto current = byId.constFind(source.consoleId);
        if (current == byId.cend() || current->kind != source.kind) {
            // A kind mismatch cannot be updated in place: OBS source types are
            // fixed at creation, so the stale one is removed and recreated.
            if (current != byId.cend()) {
                plan.removals.append(source.consoleId);
            }
            plan.creations.append(source);
            continue;
        }
        if (current->sourceName != source.sourceName) {
            plan.renames.append({source.consoleId, current->sourceName, source.sourceName});
        }
        if (current->captureKind != source.captureKind
            || current->captureDevice != source.captureDevice) {
            plan.retargets.append(source);
        }
    }
    return plan;
}

} // namespace QindaQt::ObsBridge
