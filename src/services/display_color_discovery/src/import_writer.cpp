// SPDX-License-Identifier: LGPL-3.0-or-later

#include "import_writer_p.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utility>

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

ScopedFd openValidatedRoot(const CanonicalRootContainment &containment)
{
    const QByteArray encoded = QFile::encodeName(containment.canonicalRoot());
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

ImportRootAccess::ImportRootAccess(CanonicalRootContainment containment, int fd)
    : m_containment(std::move(containment)), m_fd(fd)
{
}

std::optional<ImportRootAccess> ImportRootAccess::open(const QString &injectedRoot)
{
    std::optional<CanonicalRootContainment> containment =
        CanonicalRootContainment::resolve(injectedRoot);
    if (!containment.has_value()) {
        return std::nullopt;
    }
    ScopedFd root = openValidatedRoot(*containment);
    if (root.get() < 0) {
        return std::nullopt;
    }
    return ImportRootAccess(std::move(*containment), root.release());
}

ImportRootAccess::~ImportRootAccess()
{
    if (m_fd >= 0) {
        static_cast<void>(::close(m_fd));
    }
}

ImportRootAccess::ImportRootAccess(ImportRootAccess &&other) noexcept
    : m_containment(std::move(other.m_containment)), m_fd(std::exchange(other.m_fd, -1))
{
}

ImportRootAccess &ImportRootAccess::operator=(ImportRootAccess &&other) noexcept
{
    if (this != &other) {
        if (m_fd >= 0) {
            static_cast<void>(::close(m_fd));
        }
        m_containment = std::move(other.m_containment);
        m_fd = std::exchange(other.m_fd, -1);
    }
    return *this;
}

QString ImportRootAccess::destinationPath(const QString &fileName) const
{
    return QDir(m_containment.canonicalRoot()).filePath(fileName);
}

bool ImportRootAccess::destinationIsContained(const QString &fileName) const
{
    return m_containment.containsDestinationPath(destinationPath(fileName));
}

ExistingDestinationOutcome ImportRootAccess::inspectExisting(
    const QString &fileName, const QByteArray &content) const
{
    if (!destinationIsContained(fileName)) {
        return {ExistingDestinationOutcome::Status::Conflict, {}};
    }
    const QByteArray encodedName = QFile::encodeName(fileName);
    struct stat metadata {};
    if (::fstatat(m_fd, encodedName.constData(), &metadata, AT_SYMLINK_NOFOLLOW) != 0) {
        return errno == ENOENT
                   ? ExistingDestinationOutcome{ExistingDestinationOutcome::Status::Missing, {}}
                   : ExistingDestinationOutcome{ExistingDestinationOutcome::Status::Conflict, {}};
    }
    if (!S_ISREG(metadata.st_mode) || metadata.st_size < 0 ||
        static_cast<quint64>(metadata.st_size) != static_cast<quint64>(content.size())) {
        return {ExistingDestinationOutcome::Status::Conflict, {}};
    }
    const int rawFd =
        ::openat(m_fd, encodedName.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (rawFd < 0) {
        return {ExistingDestinationOutcome::Status::Conflict, {}};
    }
    ScopedFd file(rawFd);
    struct stat openedMetadata {};
    if (::fstat(file.get(), &openedMetadata) != 0 || !S_ISREG(openedMetadata.st_mode) ||
        openedMetadata.st_size < 0 || openedMetadata.st_size != metadata.st_size) {
        return {ExistingDestinationOutcome::Status::Conflict, {}};
    }
    const std::optional<QByteArray> storedDigest =
        readExactlyAndHash(file.get(), static_cast<size_t>(openedMetadata.st_size));
    if (!storedDigest.has_value() || *storedDigest != sha256(content)) {
        return {ExistingDestinationOutcome::Status::Conflict, {}};
    }
    return {ExistingDestinationOutcome::Status::Identical, *storedDigest};
}

ImportWriteOutcome ImportRootAccess::write(const QString &fileName,
                                           const QByteArray &content) const
{
    const QString temporaryName = temporaryNameFor(fileName);
    if (!destinationIsContained(fileName) || !destinationIsContained(temporaryName)) {
        return {ImportWriteOutcome::Status::Failed, QString(ImportWriteFailureCode)};
    }

    const QByteArray encodedName = QFile::encodeName(fileName);
    const QByteArray encodedTemporary = QFile::encodeName(temporaryName);

    // A stale temporary from an interrupted import is unlinked by name
    // without following links; a directory at the temporary name is a
    // hostile collision and fails closed.
    struct stat temporaryStat {};
    if (::fstatat(m_fd, encodedTemporary.constData(), &temporaryStat, AT_SYMLINK_NOFOLLOW) == 0) {
        if (S_ISDIR(temporaryStat.st_mode) ||
            ::unlinkat(m_fd, encodedTemporary.constData(), 0) != 0) {
            return {ImportWriteOutcome::Status::Failed, QString(ImportWriteFailureCode)};
        }
    }

    const int rawTemporary =
        ::openat(m_fd, encodedTemporary.constData(),
                 O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
    if (rawTemporary < 0) {
        return {ImportWriteOutcome::Status::Failed, QString(ImportWriteFailureCode)};
    }
    {
        ScopedFd temporary(rawTemporary);
        if (::fchmod(temporary.get(), S_IRUSR | S_IWUSR) != 0 ||
            !writeAll(temporary.get(), content) || ::fsync(temporary.get()) != 0) {
            static_cast<void>(::unlinkat(m_fd, encodedTemporary.constData(), 0));
            return {ImportWriteOutcome::Status::Failed, QString(ImportWriteFailureCode)};
        }
    }

    // AGENT-GUARD: The rename is the single commit point. Every failure
    // before it removes only this import's temporary name and leaves the
    // user root byte-identical; never replace renameat with a
    // remove-then-create sequence, which would expose a missing-profile
    // window on rejection paths.
    if (::renameat(m_fd, encodedTemporary.constData(), m_fd, encodedName.constData()) != 0) {
        static_cast<void>(::unlinkat(m_fd, encodedTemporary.constData(), 0));
        return {ImportWriteOutcome::Status::Failed, QString(ImportWriteFailureCode)};
    }
    if (!syncDirectory(m_fd)) {
        // Requested truth is visible but the directory barrier failed; the
        // import is durably uncertain. The import result reports failure so
        // the caller cannot claim provenance it has not proven, and a later
        // import of identical content converges on AlreadyPresent.
        return {ImportWriteOutcome::Status::Failed, QStringLiteral("durability-uncertain")};
    }
    return {ImportWriteOutcome::Status::Written, {}};
}

} // namespace QindaQt::DisplayColor
