// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>

#include <cstddef>
#include <string>
#include <unordered_map>

namespace QindaQt::Audio
{

// Remembers the exact module arguments PipeWire refused to load, per key, so
// the graph worker attempts a refused declaration once instead of on every
// graph rebuild. The worker uses one per family: strip and bus racks, legacy
// sends, virtual-bus loopbacks, and VBAN streams (whose "arguments" are the
// resolved declaration, since a busy port or refused route fails the start).
//
// AGENT-NOTE: a filter-chain refusal is deterministic for the same arguments
// and installed plugins, and PipeWire 1.6.8 leaks every filter-graph node it
// had already loaded when it refuses one (filter-graph.c impl_init returns
// without graph_free). The worker rebuilds on every graph event, so retrying
// there leaked hundreds of kilobytes per client stream (audio-service wiki,
// 2026-10-21). A changed declaration, a declaration that went away and came
// back, or a replaced core (clear()) each earn exactly one new attempt.
//
// Owned and used only on the WirePlumber worker thread; not thread-safe.
class ModuleRefusals final
{
public:
    // True when exactly these arguments were refused for this key and have
    // not been forgotten since.
    [[nodiscard]] bool refused(const std::string &key, const QByteArray &arguments) const;
    void remember(const std::string &key, const QByteArray &arguments);
    // Forgets every refusal whose key is no longer declared, or is declared
    // with different arguments, so the memory never outgrows the declaration.
    void retainDeclared(const std::unordered_map<std::string, QByteArray> &declared);
    void clear() noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return m_refused.size(); }

private:
    std::unordered_map<std::string, QByteArray> m_refused;
};

} // namespace QindaQt::Audio
