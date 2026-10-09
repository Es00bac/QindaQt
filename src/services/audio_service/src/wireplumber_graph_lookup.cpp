// SPDX-License-Identifier: GPL-3.0-or-later
// Name discovery stays separate from public snapshot projection. This is not
// owned-send authority: ADR0361 uses exact admitted daemon identities instead.
#include "wireplumber_graph_p.h"
#include <pipewire/keys.h>

namespace QindaQt::Audio::WirePlumberGraph
{
std::optional<NodeLookup> findNodeByName(WpObjectManager *manager,
                                         const QString &nodeName)
{
    if (manager == nullptr || nodeName.isEmpty()) {
        return std::nullopt;
    }
    const QByteArray wanted = nodeName.toUtf8();
    WpIterator *iterator = wp_object_manager_new_filtered_iterator(
        manager, WP_TYPE_NODE, nullptr);
    GValue value = G_VALUE_INIT;
    while (wp_iterator_next(iterator, &value)) {
        auto *node = WP_NODE(g_value_get_object(&value));
        const gchar *name = wp_pipewire_object_get_property(
            WP_PIPEWIRE_OBJECT(node), PW_KEY_NODE_NAME);
        if (name != nullptr && wanted == name) {
            NodeLookup result;
            result.node = WP_NODE(g_object_ref(node));
            result.boundId = wp_proxy_get_bound_id(WP_PROXY(node));
            const gchar *mediaClass = wp_pipewire_object_get_property(
                WP_PIPEWIRE_OBJECT(node), PW_KEY_MEDIA_CLASS);
            result.mediaClass =
                QString::fromLatin1(mediaClass == nullptr ? "" : mediaClass);
            result.nodeName = QString::fromUtf8(name);
            g_value_unset(&value);
            wp_iterator_unref(iterator);
            return result;
        }
        g_value_unset(&value);
    }
    wp_iterator_unref(iterator);
    return std::nullopt;
}

QStringList nodeNamesWithPrefix(WpObjectManager *manager, const QString &prefix)
{
    QStringList names;
    if (manager == nullptr || prefix.isEmpty()) {
        return names;
    }
    const QByteArray wanted = prefix.toUtf8();
    WpIterator *iterator = wp_object_manager_new_filtered_iterator(manager, WP_TYPE_NODE, nullptr);
    GValue value = G_VALUE_INIT;
    while (wp_iterator_next(iterator, &value)) {
        auto *node = WP_PIPEWIRE_OBJECT(g_value_get_object(&value));
        const gchar *name = wp_pipewire_object_get_property(node, PW_KEY_NODE_NAME);
        if (name != nullptr && g_str_has_prefix(name, wanted.constData())) {
            names.append(QString::fromUtf8(name));
        }
        g_value_unset(&value);
    }
    wp_iterator_unref(iterator);
    return names;
}

} // namespace QindaQt::Audio::WirePlumberGraph
