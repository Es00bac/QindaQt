// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_types.h>

#include <wp/wp.h>

#include <optional>

namespace QindaQt::Audio::WirePlumberGraph
{

struct BuildResult {
    Snapshot snapshot;
    bool truncatedOrMalformed = false;
};

struct NodeLookup {
    WpNode *node = nullptr;
    guint32 boundId = 0;
    QString mediaClass;
    QString nodeName;

    NodeLookup() = default;
    NodeLookup(const NodeLookup &) = delete;
    NodeLookup &operator=(const NodeLookup &) = delete;
    NodeLookup(NodeLookup &&other) noexcept;
    NodeLookup &operator=(NodeLookup &&other) noexcept;
    ~NodeLookup();
};

[[nodiscard]] quint64 daemonSerial(WpObjectManager *manager);
[[nodiscard]] WpMetadata *defaultMetadata(WpObjectManager *manager);
[[nodiscard]] BuildResult buildSnapshot(WpObjectManager *manager, WpPlugin *mixer,
                                        WpPlugin *defaultNodes, quint64 epoch,
                                        quint64 revision, Capabilities capabilities);
[[nodiscard]] std::optional<NodeLookup> findNode(WpObjectManager *manager,
                                                 quint64 serial);

} // namespace QindaQt::Audio::WirePlumberGraph
