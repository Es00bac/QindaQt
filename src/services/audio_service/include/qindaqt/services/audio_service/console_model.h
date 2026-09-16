// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_console.h>
#include <qindaqt/services/audio_protocol/audio_types.h>

#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QString>

namespace QindaQt::Audio
{

// AGENT-CONTRACT: the mixing console's own state (ADR-0173). This is the layer
// that makes QindaQt a console rather than a volume mixer: it owns the strips,
// the buses and the routing matrix between them, independently of whatever
// devices happen to exist right now.
//
// It is deliberately PURE - no PipeWire, no D-Bus, no filesystem. The service
// asks it what the console looks like and what graph the console implies; the
// backend is what actually builds that graph. That split is what makes the
// console testable without an audio daemon.
//
// Identity is stable and human-meaningful (`strip.hw.1`, `bus.a1`), never a
// graph id, so a user's routing survives a device disappearing, the service
// restarting, and the whole machine rebooting.
class ConsoleModel final
{
public:
    // The reference console's shape: 5 hardware input strips and 3 virtual
    // input strips against 5 physical output buses (A1-A5) and 3 virtual
    // output buses (B1-B3).
    static constexpr int kHardwareStrips = 5;
    static constexpr int kVirtualStrips = 3;
    static constexpr int kPhysicalBuses = 5;
    static constexpr int kVirtualBuses = 3;

    ConsoleModel();

    // The wire value, with `soloActive` derived so it can never disagree with
    // the strips.
    [[nodiscard]] Console console() const;

    // Applies one console operation. Returns false with a reason code when the
    // request names something that does not exist or carries a value outside
    // the published range; the model is left untouched in that case.
    [[nodiscard]] bool apply(const OperationRequest &request, QString *reasonCode);

    // Binds a strip or bus to a live device, or clears the binding when the
    // device is gone. Binding is separate from `apply` because it is driven by
    // the graph, not by the user.
    void bindStripSource(const QString &stripId, Handle source, bool known);
    void bindBusTarget(const QString &busId, Handle target, bool known);
    // Publishes a meter reading. Out-of-range values are dropped rather than
    // published, so a misbehaving producer cannot make a console draw a meter
    // off the top of its scale.
    void publishStripLevel(const QString &stripId, Level level);
    void publishBusLevel(const QString &busId, Level level);
    // Applies a whole batch of readings, whichever elements they name. An EMPTY
    // batch means metering has stopped and every meter goes back to unknown, so
    // a console shows an idle meter rather than freezing on its last reading.
    // Returns true when anything actually changed.
    [[nodiscard]] bool publishLevels(const QList<LevelReading> &levels);

    // The routing the console currently implies: one entry per ENABLED send,
    // in a deterministic order. The backend diffs this against the graph it has
    // built and adds or removes exactly the difference.
    struct RoutingEdge final {
        QString stripId;
        QString busId;
        quint32 busIndex = 0;
        double gainDb = 0.0;
        // False when the strip is muted, or when any strip is soloed and this
        // one is not. A silenced edge stays in the routing rather than being
        // torn down, so unsoloing restores the mix instantly instead of
        // rebuilding the graph.
        bool audible = true;

        friend bool operator==(const RoutingEdge &, const RoutingEdge &) = default;
    };
    [[nodiscard]] QList<RoutingEdge> routing() const;

    // Persistence. The document carries only the user's decisions - labels,
    // gains, mutes, routing - never live device handles or meter readings.
    [[nodiscard]] QJsonObject toJson() const;
    // Unknown or malformed entries are skipped rather than failing the whole
    // load: a console that refuses to start because one strip is corrupt is
    // worse than one that comes back with that strip at its default.
    void loadJson(const QJsonObject &document);

private:
    [[nodiscard]] Strip *findStrip(const QString &id);
    [[nodiscard]] Bus *findBus(const QString &id);
    [[nodiscard]] const Bus *findBusByIndex(quint32 index) const;

    QList<Strip> m_strips;
    QList<Bus> m_buses;
};

} // namespace QindaQt::Audio
