// SPDX-License-Identifier: GPL-3.0-or-later
#include "wireplumber_send_nodes_p.h"

#include <pipewire/keys.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <cstring>

#include <limits>

namespace QindaQt::Audio
{
namespace
{
quint64 globalNumber(WpGlobalProxy *object, const char *key)
{
    WpProperties *properties = wp_global_proxy_get_global_properties(object);
    if (properties == nullptr) {
        return 0;
    }
    const char *raw = wp_properties_get(properties, key);
    bool ok = false;
    const quint64 result = raw == nullptr || strnlen(raw, 21) > 20
        ? 0 : QString::fromLatin1(raw).toULongLong(&ok);
    wp_properties_unref(properties);
    return ok ? result : 0;
}

QString property(WpPipewireObject *node, const char *key, const size_t maximum = 64)
{
    const char *raw = wp_pipewire_object_get_property(node, key);
    if (raw == nullptr || strnlen(raw, maximum + 1) > maximum) {
        return {};
    }
    return QString::fromUtf8(raw);
}
}

SendProducerIdentity sendProducerIdentity(WpCore *core, WpObjectManager *manager,
                                         const quint64 generation)
{
    if (core == nullptr || manager == nullptr || !wp_core_is_connected(core)) {
        return {};
    }
    const quint32 clientId = wp_core_get_own_bound_id(core);
    if (clientId == 0 || clientId == std::numeric_limits<quint32>::max()) {
        return {};
    }
    auto *client = WP_CLIENT(wp_object_manager_lookup(
        manager, WP_TYPE_CLIENT, WP_CONSTRAINT_TYPE_G_PROPERTY,
        "bound-id", "=u", clientId, nullptr));
    if (client == nullptr) {
        return {};
    }
    const quint64 serial = globalNumber(WP_GLOBAL_PROXY(client), PW_KEY_OBJECT_SERIAL);
    g_object_unref(client);
    const SendProducerIdentity identity{generation, clientId, serial};
    return identity.valid() ? identity : SendProducerIdentity{};
}

QList<ObservedSendNode> observedSendNodes(WpObjectManager *manager)
{
    QList<ObservedSendNode> result;
    if (manager == nullptr) {
        return result;
    }
    WpIterator *iterator = wp_object_manager_new_filtered_iterator(manager, WP_TYPE_NODE, nullptr);
    GValue value = G_VALUE_INIT;
    constexpr qsizetype maximum = kMaxStrips * kMaxBuses * 4;
    while (wp_iterator_next(iterator, &value)) {
        auto *node = WP_PIPEWIRE_OBJECT(g_value_get_object(&value));
        const QString token = property(node, kSendInstanceProperty);
        const QString role = property(node, kSendRoleProperty);
        // Bound copied observations; unmarked public endpoints/foreign nodes
        // remain available to the strict ordinary graph mapper.
        if (token.size() == 32 && (role == QLatin1String("capture")
                                 || role == QLatin1String("playback"))) {
            const quint64 clientId = globalNumber(WP_GLOBAL_PROXY(node), PW_KEY_CLIENT_ID);
            if (clientId > 0 && clientId < std::numeric_limits<quint32>::max()) {
                if (result.size() >= maximum) {
                    // A partial list could hide duplicate roles or revive a
                    // missing pin. An empty observation revokes existing pins;
                    // worker must retire invalid modules before projection.
                    result.clear();
                    g_value_unset(&value);
                    break;
                }
                result.append({.serial = globalNumber(WP_GLOBAL_PROXY(node), PW_KEY_OBJECT_SERIAL),
                               .boundId = wp_proxy_get_bound_id(WP_PROXY(node)),
                               .objectIdentity = reinterpret_cast<quintptr>(node),
                               .clientId = static_cast<quint32>(clientId),
                               .name = property(node, PW_KEY_NODE_NAME,
                                                static_cast<size_t>(kMaxDisplayNameUtf8Bytes)),
                               .token = token, .role = role,
                               .mediaClass = property(node, PW_KEY_MEDIA_CLASS)});
            }
        }
        g_value_unset(&value);
    }
    wp_iterator_unref(iterator);
    return result;
}

} // namespace QindaQt::Audio
