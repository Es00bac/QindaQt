// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QMap>
#include <QtCore/QString>

namespace QindaQt::Audio
{

// Audio1-owned per-device latency offsets (ADR-0288), keyed by PipeWire
// `node.name`: the identity that survives a reboot or a daemon restart, when
// every serial changes. Reads are value copies; a write validates and then
// atomically replaces the whole document, so a crash leaves the previous one.
// Lifetime/threading: used serially by the coordinator on its Qt thread; the
// path is fixed at construction and no watcher is owned.
class LatencyStore final
{
public:
    explicit LatencyStore(QString path);
    // `$XDG_CONFIG_HOME/qindaqt/audio-latency.json`, or
    // QINDAQT_AUDIO_LATENCY_PATH for a probe or test run.
    [[nodiscard]] static QString defaultPath();
    // Offsets by node name, ordered by name. A missing, oversized or
    // malformed document loads empty; an invalid entry is skipped alone.
    [[nodiscard]] QMap<QString, qint64> load() const;
    // Remembers `offsetNs` for `nodeName`. Zero is stored like any other
    // value: it is the user's choice for that device and is re-applied, so a
    // reset survives WirePlumber restoring an older offset. On failure the
    // previous document is intact and `reasonCode` is a stable Audio1 code.
    [[nodiscard]] bool set(const QString &nodeName, qint64 offsetNs,
                           QString *reasonCode) const;

private:
    QString m_path;
};

} // namespace QindaQt::Audio
