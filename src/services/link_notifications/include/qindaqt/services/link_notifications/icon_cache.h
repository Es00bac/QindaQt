// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

namespace QindaQt::Link {

// Content-addressed PNG files, `<dir>/<sha256 hex>.png`; production dir is
// $XDG_CACHE_HOME/qindalink/icons. Ownership: plain value, no threads.
//
// At most `maxEntries` files are kept: store() evicts the least recently used (by modification
// time, refreshed on every has() hit), so a paired phone cannot fill the disk.
//
// AGENT-GUARD: a phone supplies both the hash and the bytes. store() accepts
// them only when the bytes are a PNG within the size bound and their SHA-256
// equals the hash, and hashes are validated before they touch a path, so a
// frame can neither write outside `dir` nor poison another app's icon.
class IconCache {
public:
    static constexpr int kDefaultMaxEntries = 256;
    explicit IconCache(QString dir, int maxEntries = kDefaultMaxEntries) : m_dir(std::move(dir)), m_max(maxEntries) {}
    [[nodiscard]] bool has(const QString &hash) const;
    // Absolute file path, or empty for an invalid hash. Does not check existence.
    [[nodiscard]] QString path(const QString &hash) const;
    // True when the file is present afterwards (also when it already was).
    [[nodiscard]] bool store(const QString &hash, const QByteArray &png);

private:
    void evictToFit() const;

    QString m_dir;
    int m_max;
};

} // namespace QindaQt::Link
