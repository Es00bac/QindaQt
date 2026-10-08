// SPDX-License-Identifier: GPL-3.0-or-later
#include "wireplumber_volume_p.h"
#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <QtCore/QSet>
#include <algorithm>
#include <cmath>
#include <limits>

namespace QindaQt::Audio::WirePlumberGraph {
QVector<double> readChannelVolumes(GVariant *dictionary)
{
    QVector<double> result;
    GVariantIter *iterator = nullptr;
    if (dictionary == nullptr
        || !g_variant_lookup(dictionary, "channelVolumes", "a{sv}", &iterator)) {
        return result;
    }
    QSet<quint32> seen;
    const gchar *key = nullptr;
    GVariant *value = nullptr;
    while (g_variant_iter_loop(iterator, "{&sv}", &key, &value)) {
        if (key == nullptr) continue;
        gchar *end = nullptr;
        const guint64 index = g_ascii_strtoull(key, &end, 10);
        if (index >= static_cast<guint64>(kMaxChannelsPerDevice) || end == key
            || end == nullptr || *end != '\0') continue;
        const auto channel = static_cast<quint32>(index);
        if (seen.contains(channel)) {
            // Ambiguous duplicate index: no channel reading is authoritative.
            result.clear();
            break;
        }
        seen.insert(channel);
        const auto slot = static_cast<qsizetype>(index);
        if (result.size() <= slot)
            result.resize(slot + 1, std::numeric_limits<double>::quiet_NaN());
        gdouble level = 0.0;
        if (g_variant_is_of_type(value, G_VARIANT_TYPE_VARDICT)
            && g_variant_lookup(value, "volume", "d", &level) && std::isfinite(level)) {
            result[slot] = std::clamp(static_cast<double>(level), 0.0, 1.0);
        }
    }
    g_variant_iter_free(iterator);
    return result;
}

VolumeState readVolume(WpPlugin *mixer, const guint32 boundId)
{
    VolumeState result;
    if (mixer == nullptr) {
        return result;
    }
    GVariant *dictionary = nullptr;
    g_signal_emit_by_name(mixer, "get-volume", boundId, &dictionary);
    if (dictionary == nullptr) {
        return result;
    }
    gdouble volume = 0.0;
    gboolean muted = FALSE;
    if (g_variant_lookup(dictionary, "volume", "d", &volume)) {
        if (std::isfinite(volume)) {
            result.volume = std::clamp(static_cast<double>(volume), 0.0, 1.0);
            result.volumeKnown = true;
            result.malformed = volume < 0.0 || volume > 1.0;
        } else {
            result.malformed = true;
        }
    }
    if (g_variant_lookup(dictionary, "mute", "b", &muted)) {
        result.muted = muted != FALSE;
        result.muteKnown = true;
    }
    result.channelVolumes = readChannelVolumes(dictionary);
    g_variant_unref(dictionary);
    return result;
}


QVector<double> projectedChannelVolumes(const VolumeState &volume,
                                        const QStringList &channelMap)
{
    // A larger mixer layout contradicts the node map. Keeping only a prefix
    // would expose a partial write as full-layout authority: mixer-api retains
    // unspecified channels. Keep aggregate controls, withhold channel truth.
    if (!channelMap.isEmpty() && volume.channelVolumes.size() > channelMap.size())
        return {};
    const qsizetype count = channelMap.isEmpty()
        ? volume.channelVolumes.size() : channelMap.size();
    QVector<double> result;
    result.reserve(count);
    for (qsizetype index = 0; index < count; ++index) {
        if (index < volume.channelVolumes.size()
            && std::isfinite(volume.channelVolumes.at(index))) {
            result.append(volume.channelVolumes.at(index));
        } else if (volume.volumeKnown) {
            result.append(volume.volume);
        } else {
            // AGENT-GUARD: A partial nonempty vector violates the strict public
            // layout contract and would withdraw unrelated graph controls.
            // Zero is a real gain, not a substitute for an unknown reading.
            return {};
        }
    }
    return result;
}
} // namespace QindaQt::Audio::WirePlumberGraph
