// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtCore/QStringList>
#include <QtCore/QVector>
#include <wp/wp.h>

namespace QindaQt::Audio::WirePlumberGraph {
// Private owning value. Only this adapter uses NaN slots to preserve missing
// mixer indices; projectedChannelVolumes never publishes such slots.
struct VolumeState {
    double volume = 0.0;
    bool volumeKnown = false;
    bool muted = false;
    bool muteKnown = false;
    QVector<double> channelVolumes;
    bool malformed = false;
};
// GLib worker thread only; borrowed dictionary/plugin, no retained references
// and no graph writes. Empty output means unknown channel truth.
[[nodiscard]] QVector<double> readChannelVolumes(GVariant *dictionary);
[[nodiscard]] VolumeState readVolume(WpPlugin *mixer, guint32 boundId);
// Pure owning projection. Node map is authoritative; missing indexed readings
// use only a known aggregate, otherwise the entire channel vector is unknown.
// Excess mixer indices contradict the mapped layout: the whole channel vector
// becomes unknown, so existing admission cannot authorize a partial write.
[[nodiscard]] QVector<double> projectedChannelVolumes(
    const VolumeState &volume, const QStringList &channelMap);
} // namespace QindaQt::Audio::WirePlumberGraph
