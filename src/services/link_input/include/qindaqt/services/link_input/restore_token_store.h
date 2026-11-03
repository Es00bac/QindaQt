// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QString>

namespace QindaQt::Link {

// The RemoteDesktop restore token (persist_mode 2) in one 0600 file inside a
// 0700 directory. The token is a credential for silent re-approval: never log
// it. Threading: GUI thread. An empty path disables persistence.
class RestoreTokenStore {
public:
    explicit RestoreTokenStore(QString path) : m_path(std::move(path)) {}
    [[nodiscard]] QString load() const;
    bool save(const QString &token) const;
    void clear() const;

private:
    QString m_path;
};

} // namespace QindaQt::Link
