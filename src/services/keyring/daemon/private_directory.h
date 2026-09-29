// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <stdexcept>
namespace qindaqt::keyring::service {
struct PersistenceError : std::runtime_error {
    PersistenceError() : std::runtime_error("Persistence not acknowledged") {}
};
// Owns a no-follow private directory descriptor plus exclusive writer flock.
// Files opened relative to pinned directory; no environment or actual user paths.
class PrivateDirectory final {
public:
    explicit PrivateDirectory(const QString &absolutePath);
    ~PrivateDirectory();
    PrivateDirectory(const PrivateDirectory &) = delete;
    QStringList collections() const;
    QByteArray read(const QString &name, int maximum) const;
    void replace(const QString &name, const QByteArray &bytes);
    void remove(const QString &name);
    int descriptor() const { return directory_; }
private:
    int directory_ = -1, lock_ = -1;
};
}
