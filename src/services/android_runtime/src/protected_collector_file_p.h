// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <sys/stat.h>
namespace QindaQt::Services::AndroidRuntime {
// Owns a descriptor opened through protected, non-symlink root-owned components.
// Re-opened fixed path must still name the exact held object on every guard.
class ProtectedCollectorFile final {
public:
    explicit ProtectedCollectorFile(QString path,bool executable);
    ~ProtectedCollectorFile();
    ProtectedCollectorFile(const ProtectedCollectorFile &)=delete;
    ProtectedCollectorFile &operator=(const ProtectedCollectorFile &)=delete;
    bool live() const;
    int duplicate() const;
    QByteArray bytes() const;
private:
    QString m_path;
    int m_fd=-1;
    struct stat m_pin{};
};
} // namespace QindaQt::Services::AndroidRuntime
