// SPDX-License-Identifier: GPL-3.0-or-later

#include "wireplumber_graph_p.h"

#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <pipewire/keys.h>

#include <QtCore/QHash>
#include <QtCore/QSet>

#include <algorithm>
#include <cmath>

namespace QindaQt::Audio::WirePlumberGraph
{
namespace
{

QString boundedText(const gchar *text, const qsizetype maxBytes,
                    const QString &fallback = {})
{
    if (text == nullptr || *text == '\0') {
        return fallback;
    }
    QByteArray bytes(text);
    if (bytes.size() > maxBytes) {
        bytes.truncate(maxBytes);
        while (!QString::fromUtf8(bytes).toUtf8().startsWith(bytes) && !bytes.isEmpty()) {
            bytes.chop(1);
        }
    }
    QString value = QString::fromUtf8(bytes);
    value.remove(QChar::Null);
    return value;
}

std::optional<quint64> serialOf(WpPipewireObject *object)
{
    const gchar *raw = wp_pipewire_object_get_property(object, PW_KEY_OBJECT_SERIAL);
    if (raw == nullptr || *raw == '\0') {
        return std::nullopt;
    }
    gchar *end = nullptr;
    const guint64 value = g_ascii_strtoull(raw, &end, 10);
    if (value == 0 || end == raw || end == nullptr || *end != '\0') {
        return std::nullopt;
    }
    return static_cast<quint64>(value);
}

bool propertyIsTrue(WpPipewireObject *object, const char *name)
{
    const gchar *value = wp_pipewire_object_get_property(object, name);
    return value != nullptr
        && (g_ascii_strcasecmp(value, "true") == 0 || g_strcmp0(value, "1") == 0);
}

struct VolumeState {
    double volume = 0.0;
    bool volumeKnown = false;
    bool muted = false;
    bool muteKnown = false;
    bool malformed = false;
};

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
    g_variant_unref(dictionary);
    return result;
}

QString preferredNodeName(WpPipewireObject *node)
{
    const gchar *name = wp_pipewire_object_get_property(node, PW_KEY_NODE_NICK);
    if (name == nullptr || *name == '\0') {
        name = wp_pipewire_object_get_property(node, PW_KEY_NODE_DESCRIPTION);
    }
    if (name == nullptr || *name == '\0') {
        name = wp_pipewire_object_get_property(node, PW_KEY_NODE_NAME);
    }
    return boundedText(name, kMaxDisplayNameUtf8Bytes, QStringLiteral("Audio device"));
}

QString description(WpPipewireObject *node)
{
    return boundedText(wp_pipewire_object_get_property(node, PW_KEY_NODE_DESCRIPTION),
                       kMaxDisplayNameUtf8Bytes);
}

QString applicationName(WpPipewireObject *node)
{
    const gchar *name = wp_pipewire_object_get_property(node, PW_KEY_APP_NAME);
    if (name == nullptr || *name == '\0') {
        name = wp_pipewire_object_get_property(node, PW_KEY_APP_PROCESS_BINARY);
    }
    return boundedText(name, kMaxApplicationNameUtf8Bytes,
                       QStringLiteral("Application"));
}

QString mediaName(WpPipewireObject *node)
{
    const gchar *name = wp_pipewire_object_get_property(node, PW_KEY_MEDIA_NAME);
    if (name == nullptr || *name == '\0') {
        name = wp_pipewire_object_get_property(node, PW_KEY_NODE_DESCRIPTION);
    }
    return boundedText(name, kMaxDisplayNameUtf8Bytes, QStringLiteral("Audio stream"));
}

bool isOutputClass(const QString &mediaClass)
{
    return mediaClass.startsWith(QStringLiteral("Audio/Sink"));
}

bool isInputClass(const QString &mediaClass)
{
    return mediaClass.startsWith(QStringLiteral("Audio/Source"));
}

bool isPlaybackClass(const QString &mediaClass)
{
    return mediaClass.startsWith(QStringLiteral("Stream/Output/Audio"));
}

bool isCaptureClass(const QString &mediaClass)
{
    return mediaClass.startsWith(QStringLiteral("Stream/Input/Audio"));
}

} // namespace

NodeLookup::NodeLookup(NodeLookup &&other) noexcept
    : node(std::exchange(other.node, nullptr))
    , boundId(other.boundId)
    , mediaClass(std::move(other.mediaClass))
    , nodeName(std::move(other.nodeName))
{
}

NodeLookup &NodeLookup::operator=(NodeLookup &&other) noexcept
{
    if (this != &other) {
        if (node != nullptr) {
            g_object_unref(node);
        }
        node = std::exchange(other.node, nullptr);
        boundId = other.boundId;
        mediaClass = std::move(other.mediaClass);
        nodeName = std::move(other.nodeName);
    }
    return *this;
}

NodeLookup::~NodeLookup()
{
    if (node != nullptr) {
        g_object_unref(node);
    }
}

quint64 daemonSerial(WpObjectManager *manager)
{
    if (manager == nullptr) {
        return 0;
    }
    WpIterator *iterator = wp_object_manager_new_filtered_iterator(
        manager, WP_TYPE_CLIENT, nullptr);
    GValue value = G_VALUE_INIT;
    quint64 result = 0;
    while (wp_iterator_next(iterator, &value)) {
        auto *client = WP_PIPEWIRE_OBJECT(g_value_get_object(&value));
        if (propertyIsTrue(client, "wireplumber.daemon")) {
            result = serialOf(client).value_or(0);
            g_value_unset(&value);
            break;
        }
        g_value_unset(&value);
    }
    wp_iterator_unref(iterator);
    return result;
}

WpMetadata *defaultMetadata(WpObjectManager *manager)
{
    if (manager == nullptr) {
        return nullptr;
    }
    return WP_METADATA(wp_object_manager_lookup(
        manager, WP_TYPE_METADATA, WP_CONSTRAINT_TYPE_PW_GLOBAL_PROPERTY,
        "metadata.name", "=s", "default", nullptr));
}

BuildResult buildSnapshot(WpObjectManager *manager, WpPlugin *mixer,
                          WpPlugin *defaultNodes, const quint64 epoch,
                          const quint64 revision, const Capabilities capabilities)
{
    BuildResult result;
    result.snapshot.schemaVersion = kSchemaVersion;
    result.snapshot.epoch = epoch;
    result.snapshot.revision = revision;
    result.snapshot.availability = Availability::Ready;
    result.snapshot.capabilities = capabilities;

    guint defaultOutputId = G_MAXUINT;
    guint defaultInputId = G_MAXUINT;
    if (defaultNodes != nullptr) {
        g_signal_emit_by_name(defaultNodes, "get-default-node", "Audio/Sink",
                              &defaultOutputId);
        g_signal_emit_by_name(defaultNodes, "get-default-node", "Audio/Source",
                              &defaultInputId);
    }

    QHash<guint32, quint64> boundToSerial;
    QSet<quint64> retainedSerials;
    WpIterator *iterator = wp_object_manager_new_filtered_iterator(
        manager, WP_TYPE_NODE, nullptr);
    GValue value = G_VALUE_INIT;
    while (wp_iterator_next(iterator, &value)) {
        auto *node = WP_PIPEWIRE_OBJECT(g_value_get_object(&value));
        const auto serial = serialOf(node);
        const gchar *rawClass = wp_pipewire_object_get_property(node, PW_KEY_MEDIA_CLASS);
        const QString mediaClass = QString::fromLatin1(rawClass == nullptr ? "" : rawClass);
        if (!serial.has_value()) {
            result.truncatedOrMalformed = true;
            g_value_unset(&value);
            continue;
        }
        const bool output = isOutputClass(mediaClass);
        const bool input = isInputClass(mediaClass);
        const bool playback = isPlaybackClass(mediaClass);
        const bool capture = isCaptureClass(mediaClass);
        if (!output && !input && !playback && !capture) {
            g_value_unset(&value);
            continue;
        }
        const bool atCapacity = (output && result.snapshot.outputs.size() >= kMaxOutputs)
            || (input && result.snapshot.inputs.size() >= kMaxInputs)
            || ((playback || capture) && result.snapshot.streams.size() >= kMaxStreams);
        if (atCapacity) {
            result.truncatedOrMalformed = true;
            g_value_unset(&value);
            continue;
        }
        if (retainedSerials.contains(*serial)) {
            result.truncatedOrMalformed = true;
            g_value_unset(&value);
            continue;
        }
        const guint32 boundId = wp_proxy_get_bound_id(WP_PROXY(node));
        boundToSerial.insert(boundId, *serial);
        retainedSerials.insert(*serial);
        const VolumeState volume = readVolume(mixer, boundId);
        result.truncatedOrMalformed = result.truncatedOrMalformed || volume.malformed;

        if (output || input) {
            Device device;
            device.handle = {.epoch = epoch, .serial = *serial};
            device.kind = output ? DeviceKind::Output : DeviceKind::Input;
            device.name = preferredNodeName(node);
            device.description = description(node);
            device.volume = volume.volume;
            device.volumeKnown = volume.volumeKnown;
            device.muted = volume.muted;
            device.muteKnown = volume.muteKnown;
            device.isDefault = boundId
                == (device.kind == DeviceKind::Output ? defaultOutputId : defaultInputId);
            device.canSetVolume = volume.volumeKnown && mixer != nullptr;
            device.canSetMute = volume.muteKnown && mixer != nullptr;
            if (device.kind == DeviceKind::Output) {
                result.snapshot.outputs.push_back(std::move(device));
            } else {
                result.snapshot.inputs.push_back(std::move(device));
            }
        } else {
            Stream stream;
            stream.handle = {.epoch = epoch, .serial = *serial};
            stream.direction = playback ? StreamDirection::Playback
                                        : StreamDirection::Capture;
            stream.applicationName = applicationName(node);
            stream.mediaName = mediaName(node);
            stream.volume = volume.volume;
            stream.volumeKnown = volume.volumeKnown;
            stream.muted = volume.muted;
            stream.muteKnown = volume.muteKnown;
            stream.canSetVolume = volume.volumeKnown && mixer != nullptr;
            stream.canSetMute = volume.muteKnown && mixer != nullptr;
            stream.canMove = capabilities.testFlag(Capability::MoveStream);
            result.snapshot.streams.push_back(std::move(stream));
        }
        g_value_unset(&value);
    }
    wp_iterator_unref(iterator);

    constexpr qsizetype maxObservedLinks = kMaxStreams * 8;
    qsizetype observedLinks = 0;
    WpIterator *links = wp_object_manager_new_filtered_iterator(manager, WP_TYPE_LINK, nullptr);
    while (wp_iterator_next(links, &value)) {
        if (observedLinks >= maxObservedLinks) {
            result.truncatedOrMalformed = true;
            g_value_unset(&value);
            break;
        }
        ++observedLinks;
        auto *link = WP_LINK(g_value_get_object(&value));
        guint32 outputNode = 0;
        guint32 inputNode = 0;
        wp_link_get_linked_object_ids(link, &outputNode, nullptr, &inputNode, nullptr);
        const quint64 outputSerial = boundToSerial.value(outputNode, 0);
        const quint64 inputSerial = boundToSerial.value(inputNode, 0);
        for (Stream &stream : result.snapshot.streams) {
            quint64 targetSerial = 0;
            if (stream.direction == StreamDirection::Playback
                && stream.handle.serial == outputSerial) {
                targetSerial = inputSerial;
            } else if (stream.direction == StreamDirection::Capture
                       && stream.handle.serial == inputSerial) {
                targetSerial = outputSerial;
            }
            if (targetSerial != 0) {
                stream.target = {.epoch = epoch, .serial = targetSerial};
                stream.targetKnown = true;
            }
        }
        g_value_unset(&value);
    }
    wp_iterator_unref(links);

    const auto sortBySerial = [](const auto &left, const auto &right) {
        return left.handle.serial < right.handle.serial;
    };
    std::sort(result.snapshot.outputs.begin(), result.snapshot.outputs.end(), sortBySerial);
    std::sort(result.snapshot.inputs.begin(), result.snapshot.inputs.end(), sortBySerial);
    std::sort(result.snapshot.streams.begin(), result.snapshot.streams.end(), sortBySerial);

    QSet<quint64> retainedOutputs;
    QSet<quint64> retainedInputs;
    for (const Device &device : result.snapshot.outputs) {
        retainedOutputs.insert(device.handle.serial);
    }
    for (const Device &device : result.snapshot.inputs) {
        retainedInputs.insert(device.handle.serial);
    }
    for (Stream &stream : result.snapshot.streams) {
        const QSet<quint64> &targets = stream.direction == StreamDirection::Playback
            ? retainedOutputs
            : retainedInputs;
        if (stream.targetKnown && !targets.contains(stream.target.serial)) {
            stream.target = {};
            stream.targetKnown = false;
            result.truncatedOrMalformed = true;
        }
    }

    for (const Device &device : result.snapshot.outputs) {
        if (device.isDefault) {
            result.snapshot.defaultOutput = device.handle;
            break;
        }
    }
    for (const Device &device : result.snapshot.inputs) {
        if (device.isDefault) {
            result.snapshot.defaultInput = device.handle;
            break;
        }
    }
    if (result.truncatedOrMalformed) {
        result.snapshot.availability = Availability::Degraded;
        result.snapshot.reasonCode = QStringLiteral("graph-bounded");
    }
    return result;
}

std::optional<NodeLookup> findNode(WpObjectManager *manager, const quint64 serial)
{
    if (manager == nullptr || serial == 0) {
        return std::nullopt;
    }
    WpIterator *iterator = wp_object_manager_new_filtered_iterator(
        manager, WP_TYPE_NODE, nullptr);
    GValue value = G_VALUE_INIT;
    while (wp_iterator_next(iterator, &value)) {
        auto *node = WP_NODE(g_value_get_object(&value));
        if (serialOf(WP_PIPEWIRE_OBJECT(node)).value_or(0) == serial) {
            NodeLookup result;
            result.node = WP_NODE(g_object_ref(node));
            result.boundId = wp_proxy_get_bound_id(WP_PROXY(node));
            const gchar *mediaClass = wp_pipewire_object_get_property(
                WP_PIPEWIRE_OBJECT(node), PW_KEY_MEDIA_CLASS);
            const gchar *nodeName = wp_pipewire_object_get_property(
                WP_PIPEWIRE_OBJECT(node), PW_KEY_NODE_NAME);
            result.mediaClass = QString::fromLatin1(mediaClass == nullptr ? "" : mediaClass);
            result.nodeName = QString::fromUtf8(nodeName == nullptr ? "" : nodeName);
            g_value_unset(&value);
            wp_iterator_unref(iterator);
            return result;
        }
        g_value_unset(&value);
    }
    wp_iterator_unref(iterator);
    return std::nullopt;
}

} // namespace QindaQt::Audio::WirePlumberGraph
