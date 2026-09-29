// SPDX-License-Identifier: GPL-3.0-or-later

#include "wireplumber_latency_p.h"

#include <spa/param/props.h>
#include <spa/pod/iter.h>

namespace QindaQt::Audio::WirePlumberLatency
{
namespace
{

// Visits each object pod an enumeration yielded until `visit` answers true.
template <typename Visit>
void forEachObject(WpIterator *params, Visit &&visit)
{
    GValue value = G_VALUE_INIT;
    bool done = false;
    while (!done && wp_iterator_next(params, &value)) {
        auto *pod = static_cast<WpSpaPod *>(g_value_get_boxed(&value));
        const struct spa_pod *raw = pod == nullptr ? nullptr : wp_spa_pod_get_spa_pod(pod);
        if (raw != nullptr && spa_pod_is_object(raw)) {
            done = visit(raw);
        }
        g_value_unset(&value);
    }
}

} // namespace

std::optional<qint64> readOffset(WpPipewireObject *node)
{
    std::optional<qint64> offset;
    WpIterator *params = wp_pipewire_object_enum_params_sync(node, "Props", nullptr);
    if (params == nullptr) {
        return offset;
    }
    // An adapter node reports its converter's and its device's Props; only
    // the device half carries the offset.
    forEachObject(params, [&offset](const struct spa_pod *object) {
        const struct spa_pod_prop *prop =
            spa_pod_find_prop(object, nullptr, SPA_PROP_latencyOffsetNsec);
        int64_t nanoseconds = 0;
        if (prop != nullptr && spa_pod_get_long(&prop->value, &nanoseconds) == 0) {
            offset = nanoseconds;
        }
        return offset.has_value();
    });
    wp_iterator_unref(params);
    return offset;
}

std::optional<LatencyPolicy::Range> parseRange(WpIterator *propInfo)
{
    std::optional<LatencyPolicy::Range> range;
    if (propInfo == nullptr) {
        return range;
    }
    forEachObject(propInfo, [&range](const struct spa_pod *object) {
        const struct spa_pod_prop *id = spa_pod_find_prop(object, nullptr, SPA_PROP_INFO_id);
        const struct spa_pod_prop *type =
            spa_pod_find_prop(object, nullptr, SPA_PROP_INFO_type);
        uint32_t key = 0;
        if (id == nullptr || type == nullptr || spa_pod_get_id(&id->value, &key) != 0
            || key != SPA_PROP_latencyOffsetNsec) {
            return false;
        }
        // AGENT-NOTE: a Range choice lists default, minimum, maximum. ALSA
        // declares 0..2 s and Bluetooth sinks the whole signed range; any
        // other shape means the node offers no range Audio1 can honour.
        uint32_t count = 0;
        uint32_t choice = 0;
        const struct spa_pod *values = spa_pod_get_values(&type->value, &count, &choice);
        if (values != nullptr && values->type == SPA_TYPE_Long && choice == SPA_CHOICE_Range
            && count >= 3) {
            const auto *bounds = static_cast<const int64_t *>(SPA_POD_BODY_CONST(values));
            if (bounds[1] <= bounds[2]) {
                range = LatencyPolicy::Range{.minNs = bounds[1], .maxNs = bounds[2]};
            }
        }
        return true;
    });
    return range;
}

bool writeOffset(WpPipewireObject *node, const qint64 offsetNs)
{
    WpSpaPod *props = wp_spa_pod_new_object("Spa:Pod:Object:Param:Props", "Props",
                                            "latencyOffsetNsec", "l",
                                            static_cast<gint64>(offsetNs), nullptr);
    if (props == nullptr) {
        return false;
    }
    // set_param takes ownership of the pod.
    return wp_pipewire_object_set_param(node, "Props", 0, props) != FALSE;
}

} // namespace QindaQt::Audio::WirePlumberLatency
