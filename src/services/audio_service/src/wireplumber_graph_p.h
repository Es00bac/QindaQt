// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_types.h>

#include <QtCore/QStringList>

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
// Finds a node by its `node.name`. The console's send loopbacks are addressed
// this way because their names are deterministic (ADR-0173) while their object
// serials are assigned by PipeWire when the module loads.
[[nodiscard]] std::optional<NodeLookup> findNodeByName(WpObjectManager *manager,
                                                       const QString &nodeName);
// Every node whose `node.name` starts with the prefix. The console uses it to
// find its own leftovers - a lingering sink from a previous run - which no
// proxy of this run can name.
[[nodiscard]] QStringList nodeNamesWithPrefix(WpObjectManager *manager,
                                              const QString &prefix);

} // namespace QindaQt::Audio::WirePlumberGraph
