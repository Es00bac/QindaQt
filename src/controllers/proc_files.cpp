// SPDX-License-Identifier: GPL-3.0-or-later
#include "proc_files_p.h"
#include <QFile>
#include <QThread>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace QindaQt::Controllers::ProcFiles {
namespace {
// Owns one descriptor. O_NOFOLLOW refuses a symlink in the final component;
// O_NONBLOCK keeps an unexpected FIFO or device from blocking the worker.
class Descriptor {
public:
    explicit Descriptor(const QString &path)
        : m_fd(::open(QFile::encodeName(path).constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY)) {}
    ~Descriptor() { if (m_fd >= 0) ::close(m_fd); }
    Descriptor(const Descriptor &) = delete;
    Descriptor &operator=(const Descriptor &) = delete;
    bool valid() const { return m_fd >= 0; }
    // Bytes read, 0 at end of file, -1 on a real error. EINTR is retried.
    qsizetype read(char *data, qsizetype size) const {
        for (;;) {
            const auto count = ::read(m_fd, data, static_cast<size_t>(size));
            if (count >= 0) return static_cast<qsizetype>(count);
            if (errno != EINTR) return -1;
        }
    }
private:
    int m_fd;
};
}
QByteArray read(const QString &path, qint64 limit) {
    const Descriptor file(path);
    if (!file.valid()) return {};
    QByteArray data;
    qint64 total = 0;
    while (total < limit) {
        if (QThread::currentThread()->isInterruptionRequested()) break;
        const auto want = static_cast<qsizetype>(std::min<qint64>(ChunkBytes, limit - total));
        const auto used = data.size();
        data.resize(used + want);
        const auto got = file.read(data.data() + used, want);
        data.resize(used + std::max<qsizetype>(got, 0));
        if (got <= 0) break;
        total += got;
    }
    return data;
}
quint32 findAny(const QString &path, const QList<QByteArray> &needles, quint32 stopMask, qint64 limit) {
    const Descriptor file(path);
    if (!file.valid() || needles.isEmpty() || needles.size() > 32) return 0;
    qsizetype overlap = 0;
    for (const auto &needle : needles) overlap = std::max(overlap, needle.size() - 1);
    // Layout: [carry: tail of the previous chunk][fresh bytes]. The carry makes
    // a needle that straddles two reads findable without re-reading the file.
    QByteArray buffer(overlap + ChunkBytes, Qt::Uninitialized);
    qsizetype carry = 0;
    qint64 total = 0;
    quint32 found = 0;
    while (total < limit) {
        if (QThread::currentThread()->isInterruptionRequested()) break;
        const auto want = static_cast<qsizetype>(std::min<qint64>(ChunkBytes, limit - total));
        const auto got = file.read(buffer.data() + carry, want);
        if (got <= 0) break;
        total += got;
        const QByteArrayView window(buffer.constData(), carry + got);
        for (qsizetype i = 0; i < needles.size(); ++i)
            if (window.indexOf(needles[i]) >= 0) found |= quint32(1) << i;
        if (found & stopMask) break;
        carry = std::min(overlap, window.size());
        std::memmove(buffer.data(), buffer.constData() + window.size() - carry, static_cast<size_t>(carry));
    }
    return found;
}
QString linkTarget(const QString &path) {
    std::array<char, 4096> target{};
    const auto size = ::readlink(QFile::encodeName(path).constData(), target.data(), target.size());
    if (size <= 0 || size == static_cast<ssize_t>(target.size())) return {};
    return QFile::decodeName(QByteArray(target.data(), static_cast<qsizetype>(size)));
}
QString cgroupPath(const QString &processDirectory) {
    for (const auto &line : read(processDirectory + "/cgroup", SmallLimit).split('\n'))
        if (line.startsWith("0::")) return QString::fromUtf8(line.mid(3)).trimmed();
    return {};
}
} // namespace QindaQt::Controllers::ProcFiles
