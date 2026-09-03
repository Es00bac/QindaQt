// SPDX-License-Identifier: LGPL-3.0-or-later

#include "import_writer_p.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace QindaQt::DisplayColor
{
namespace
{

QString temporaryNameFor(const QString &fileName)
{
    return QChar(u'.') + fileName + QStringLiteral(".qindaqt-import-tmp");
}

class ScopedFd
{
public:
    ScopedFd() = default;
    explicit ScopedFd(int fd) noexcept : m_fd(fd) {}
    ~ScopedFd()
    {
        if (m_fd >= 0) {
            static_cast<void>(::close(m_fd));
        }
    }
    ScopedFd(const ScopedFd &) = delete;
    ScopedFd &operator=(const ScopedFd &) = delete;
    ScopedFd(ScopedFd &&other) noexcept : m_fd(other.release()) {}
    ScopedFd &operator=(ScopedFd &&other) noexcept
    {
        if (this != &other) {
            if (m_fd >= 0) {
                static_cast<void>(::close(m_fd));
            }
            m_fd = other.release();
        }
        return *this;
    }
    [[nodiscard]] int get() const noexcept { return m_fd; }
    [[nodiscard]] int release() noexcept
    {
        const int value = m_fd;
        m_fd = -1;
        return value;
    }

private:
    int m_fd = -1;
};

// AGENT-GUARD: The persistence root is validated exactly like the ADR-0051
// journal root — non-symlink, effective-user-owned, not group/other-writable.
// Relaxing this would let a redirected or shared-writable directory become
// assignment-provenance authority.
ScopedFd openValidatedRoot(const QString &root)
{
    if (root.isEmpty() || root.contains(QChar(u'\0'))) {
        return {};
    }
    const QByteArray encoded = QFile::encodeName(root);
    ScopedFd fd(::open(encoded.constData(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW));
    if (fd.get() < 0) {
        return {};
    }
    struct stat metadata {};
    if (::fstat(fd.get(), &metadata) != 0 || !S_ISDIR(metadata.st_mode) ||
        metadata.st_uid != ::geteuid() || (metadata.st_mode & 0022) != 0) {
        return {};
    }
    return fd;
}

bool writeAll(int fd, const QByteArray &payload)
{
    qsizetype offset = 0;
    while (offset < payload.size()) {
        const ssize_t written =
            ::write(fd, payload.constData() + offset, static_cast<size_t>(payload.size() - offset));
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (written == 0) {
            return false;
        }
        offset += written;
    }
    return true;
}

bool syncDirectory(int rootFd)
{
    if (::fsync(rootFd) == 0) {
        return true;
    }
    // Some atomic filesystems reject directory fsync; the rename itself
    // remains atomic there, only the durability barrier is unsupported.
    return errno == EINVAL || errno == ENOTSUP || errno == EOPNOTSUPP;
}

QByteArray sha256(const QByteArray &payload)
{
    return QCryptographicHash::hash(payload, QCryptographicHash::Sha256);
}

// Reads exactly payloadSize bytes from fd (which must already be positioned
// at the start) and hashes them; nullopt on any short read, error, or
// trailing data.
std::optional<QByteArray> readExactlyAndHash(int fd, size_t payloadSize)
{
    QByteArray payload(static_cast<qsizetype>(payloadSize), Qt::Uninitialized);
    size_t totalRead = 0;
    while (totalRead < payloadSize) {
        const ssize_t count =
            ::read(fd, payload.data() + static_cast<qsizetype>(totalRead),
                   static_cast<size_t>(payloadSize - totalRead));
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return std::nullopt;
        }
        if (count == 0) {
            return std::nullopt;
        }
        totalRead += static_cast<size_t>(count);
    }
    char trailing = 0;
    const ssize_t extra = ::read(fd, &trailing, 1);
    if (extra != 0) {
        // Extra == -1 (error) is treated as hostile too: provenance must be
        // proven, never assumed on a partially failed read.
        return std::nullopt;
    }
    return sha256(payload);
}

} // namespace

ImportWriteOutcome atomicWriteProfileCopy(const QString &userRoot, const QString &fileName,
                                          const QByteArray &content)
{
    const ScopedFd root = openValidatedRoot(userRoot);
    if (root.get() < 0) {
        return {ImportWriteOutcome::Status::Failed, QString(ImportRootUnsafeCode)};
    }

    const QByteArray encodedName = QFile::encodeName(fileName);
    const QByteArray encodedTemporary = QFile::encodeName(temporaryNameFor(fileName));

    // A stale temporary from an interrupted import is unlinked by name
    // without following links; a directory at the temporary name is a
    // hostile collision and fails closed.
    struct stat temporaryStat {};
    if (::fstatat(root.get(), encodedTemporary.constData(), &temporaryStat, AT_SYMLINK_NOFOLLOW) == 0) {
        if (S_ISDIR(temporaryStat.st_mode) ||
            ::unlinkat(root.get(), encodedTemporary.constData(), 0) != 0) {
            return {ImportWriteOutcome::Status::Failed, QString(ImportWriteFailureCode)};
        }
    }

    const int rawTemporary =
        ::openat(root.get(), encodedTemporary.constData(),
                 O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
    if (rawTemporary < 0) {
        return {ImportWriteOutcome::Status::Failed, QString(ImportWriteFailureCode)};
    }
    {
        ScopedFd temporary(rawTemporary);
        if (::fchmod(temporary.get(), S_IRUSR | S_IWUSR) != 0 ||
            !writeAll(temporary.get(), content) || ::fsync(temporary.get()) != 0) {
            static_cast<void>(::unlinkat(root.get(), encodedTemporary.constData(), 0));
            return {ImportWriteOutcome::Status::Failed, QString(ImportWriteFailureCode)};
        }
    }

    // AGENT-GUARD: The rename is the single commit point. Every failure
    // before it removes only this import's temporary name and leaves the
    // user root byte-identical; never replace renameat with a
    // remove-then-create sequence, which would expose a missing-profile
    // window on rejection paths.
    if (::renameat(root.get(), encodedTemporary.constData(), root.get(), encodedName.constData()) != 0) {
        static_cast<void>(::unlinkat(root.get(), encodedTemporary.constData(), 0));
        return {ImportWriteOutcome::Status::Failed, QString(ImportWriteFailureCode)};
    }
    if (!syncDirectory(root.get())) {
        // Requested truth is visible but the directory barrier failed; the
        // import is durably uncertain. The import result reports failure so
        // the caller cannot claim provenance it has not proven, and a later
        // import of identical content converges on AlreadyPresent.
        return {ImportWriteOutcome::Status::Failed, QStringLiteral("durability-uncertain")};
    }
    return {ImportWriteOutcome::Status::Written, {}};
}

bool removeStaleImportTemporary(const QString &userRoot, const QString &fileName)
{
    const ScopedFd root = openValidatedRoot(userRoot);
    if (root.get() < 0) {
        return false;
    }
    const QByteArray encodedTemporary = QFile::encodeName(temporaryNameFor(fileName));
    struct stat metadata {};
    if (::fstatat(root.get(), encodedTemporary.constData(), &metadata, AT_SYMLINK_NOFOLLOW) != 0) {
        return errno == ENOENT;
    }
    return !S_ISDIR(metadata.st_mode) &&
           ::unlinkat(root.get(), encodedTemporary.constData(), 0) == 0;
}

std::optional<QByteArray> existingFileDigestIfIdentical(const QString &root, const QString &fileName,
                                                        const QByteArray &content)
{
    const ScopedFd rootFd = openValidatedRoot(root);
    if (rootFd.get() < 0) {
        return std::nullopt;
    }
    const QByteArray encodedName = QFile::encodeName(fileName);
    // Open without following links so a planted symlink can never stand in
    // for the stored profile, then re-stat the opened descriptor.
    const int rawFd = ::openat(rootFd.get(), encodedName.constData(),
                               O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (rawFd < 0) {
        return std::nullopt;
    }
    ScopedFd file(rawFd);
    struct stat metadata {};
    if (::fstat(file.get(), &metadata) != 0 || !S_ISREG(metadata.st_mode)) {
        return std::nullopt;
    }
    if (metadata.st_size < 0 ||
        static_cast<quint64>(metadata.st_size) != static_cast<quint64>(content.size())) {
        return std::nullopt;
    }
    const std::optional<QByteArray> storedDigest =
        readExactlyAndHash(file.get(), static_cast<size_t>(metadata.st_size));
    if (!storedDigest.has_value()) {
        return std::nullopt;
    }
    const QByteArray contentDigest = sha256(content);
    if (*storedDigest != contentDigest) {
        return std::nullopt;
    }
    return contentDigest;
}

} // namespace QindaQt::DisplayColor
