// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/audio_protocol/audio_types.h>

#include <QList>
#include <QString>

#include <optional>

// AGENT-CONTRACT: the pure half of the OBS console bridge (ADR-0208). It
// projects one Audio1 snapshot onto the OBS sources the console should be
// visible as, and diffs that against the sources OBS already holds. It knows
// nothing about libobs, so the sync rules are testable without a running
// OBS; the module applies the plan through the libobs C API.
namespace QindaQt::ObsBridge {

// The two OBS source types the module registers.
enum class SourceKind { Bus, Strip };

// How a source captures its audio through the PulseAudio protocol that
// PipeWire serves: `Input` records a real source node (a virtual bus's
// `.source`, a hardware strip's capture device), `Monitor` records a sink's
// monitor (a physical bus's target device, a virtual strip's sink). `None`
// means the console has no device for it yet; the source exists, silent.
enum class CaptureKind { None, Input, Monitor };

// Keys and tokens carried in the OBS source settings: the bridge's public
// contract with obs-websocket clients (ADR-0208).
namespace SettingsKeys {
inline constexpr char ConsoleId[] = "qindaqt_console_id";
inline constexpr char Code[] = "qindaqt_code";
inline constexpr char Label[] = "qindaqt_label";
inline constexpr char CaptureDevice[] = "qindaqt_capture_device";
inline constexpr char CaptureKind[] = "qindaqt_capture_kind";
} // namespace SettingsKeys

inline constexpr char BusSourceId[] = "qindaqt_console_bus";
inline constexpr char StripSourceId[] = "qindaqt_console_strip";
inline constexpr char VendorName[] = "qindaqt";
inline constexpr char MappingRequest[] = "GetConsoleMapping";
inline constexpr char MappingChangedEvent[] = "ConsoleMappingChanged";
inline constexpr int BridgeVersion = 1;

[[nodiscard]] QString sourceKindId(SourceKind kind);
[[nodiscard]] std::optional<SourceKind> sourceKindFromId(const QString &id);
[[nodiscard]] QString captureKindToken(CaptureKind kind);
[[nodiscard]] std::optional<CaptureKind> captureKindFromToken(const QString &token);

// One source the console should be visible as.
struct DesiredSource {
    QString consoleId;
    SourceKind kind = SourceKind::Bus;
    // The short console code people know: "A1", "B2", "Virtual 1".
    QString code;
    // The user's label for the strip or bus (may be empty).
    QString label;
    // The unique OBS source name.
    QString sourceName;
    CaptureKind captureKind = CaptureKind::None;
    QString captureDevice;
    bool muted = false;
    double gainDb = 0.0;
    friend bool operator==(const DesiredSource &, const DesiredSource &) = default;
};

// One source OBS already holds, as read back from its settings.
struct ExistingSource {
    QString consoleId;
    SourceKind kind = SourceKind::Bus;
    QString sourceName;
    CaptureKind captureKind = CaptureKind::None;
    QString captureDevice;
    friend bool operator==(const ExistingSource &, const ExistingSource &) = default;
};

// The console code for an id: "bus.a1" -> "A1", "strip.virtual.1" ->
// "Virtual 1"; falls back to the kind letter plus 1-based index.
[[nodiscard]] QString busCode(const Audio::Bus &bus);
[[nodiscard]] QString stripCode(const Audio::Strip &strip);
// "QindaQt Bus A1", "QindaQt Bus A1 — Speakers", "QindaQt Strip Virtual 1".
[[nodiscard]] QString sourceNameFor(SourceKind kind, const QString &code, const QString &label);
// The PipeWire node name the console gives an endpoint: the service's
// `qindaqt.console.` prefix plus the console id.
[[nodiscard]] QString consoleNodeName(const QString &consoleId);

// Every bus, then every strip, in console order, with unique source names.
[[nodiscard]] QList<DesiredSource> desiredSources(const Audio::Snapshot &snapshot);

struct Rename {
    QString consoleId;
    QString from;
    QString to;
    friend bool operator==(const Rename &, const Rename &) = default;
};

// The operations that turn `existing` into `desired`, applied in this order:
// removals free names, renames settle names, retargets follow devices,
// creations fill the gaps. Keyed by console id throughout.
struct SyncPlan {
    QList<QString> removals;
    QList<Rename> renames;
    QList<DesiredSource> retargets;
    QList<DesiredSource> creations;
    [[nodiscard]] bool empty() const
    {
        return removals.isEmpty() && renames.isEmpty() && retargets.isEmpty()
            && creations.isEmpty();
    }
    friend bool operator==(const SyncPlan &, const SyncPlan &) = default;
};

[[nodiscard]] SyncPlan planSync(const QList<DesiredSource> &desired,
                                const QList<ExistingSource> &existing);

} // namespace QindaQt::ObsBridge
