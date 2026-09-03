// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_color_discovery/profile_discovery.h>

#include "icc_text_metadata_p.h"
#include "path_safety_p.h"
#include "profile_import_p.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QtEndian>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace QindaQt::DisplayColor
{
namespace
{

constexpr qsizetype MaxDiagnosticTextChars = 512;

QString bounded(QString text)
{
    if (text.size() > MaxDiagnosticTextChars) {
        text.resize(MaxDiagnosticTextChars);
    }
    return text;
}

DiscoveryDiagnostic diagnostic(DiscoverySeverity severity, QString code, QString path,
                               QString detail = {})
{
    return DiscoveryDiagnostic{severity, std::move(code), bounded(std::move(path)),
                               bounded(std::move(detail))};
}

bool validOrigin(DiscoveryOrigin origin)
{
    switch (origin) {
    case DiscoveryOrigin::BuiltIn:
    case DiscoveryOrigin::System:
    case DiscoveryOrigin::UserImported:
        return true;
    }
    return false;
}

enum class ContentComparison
{
    Identical,
    DifferentOrUnverifiable,
};

ssize_t readRetry(int fd, char *buffer, size_t size)
{
    ssize_t count = 0;
    do {
        count = ::read(fd, buffer, size);
    } while (count < 0 && errno == EINTR);
    return count;
}

ContentComparison compareProfileContent(const QString &leftPath, const QString &rightPath)
{
    const QByteArray leftName = QFile::encodeName(leftPath);
    const QByteArray rightName = QFile::encodeName(rightPath);
    const int leftFd = ::open(leftName.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (leftFd < 0) {
        return ContentComparison::DifferentOrUnverifiable;
    }
    const int rightFd = ::open(rightName.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (rightFd < 0) {
        static_cast<void>(::close(leftFd));
        return ContentComparison::DifferentOrUnverifiable;
    }

    struct stat leftStat {};
    struct stat rightStat {};
    const bool safe = ::fstat(leftFd, &leftStat) == 0 && ::fstat(rightFd, &rightStat) == 0 &&
                      S_ISREG(leftStat.st_mode) && S_ISREG(rightStat.st_mode) &&
                      leftStat.st_size >= 0 && leftStat.st_size == rightStat.st_size &&
                      static_cast<quint64>(leftStat.st_size) <= MaxIccProfileSizeBytes;
    bool identical = safe;
    std::array<char, 64 * 1024> leftChunk {};
    std::array<char, 64 * 1024> rightChunk {};
    quint64 remaining = safe ? static_cast<quint64>(leftStat.st_size) : 0;
    while (identical && remaining > 0) {
        const size_t requested = static_cast<size_t>(
            std::min<quint64>(remaining, static_cast<quint64>(leftChunk.size())));
        const ssize_t leftRead = readRetry(leftFd, leftChunk.data(), requested);
        const ssize_t rightRead = readRetry(rightFd, rightChunk.data(), requested);
        if (leftRead != static_cast<ssize_t>(requested) ||
            rightRead != static_cast<ssize_t>(requested)) {
            identical = false;
            break;
        }
        if (!std::equal(leftChunk.begin(),
                        leftChunk.begin() + static_cast<std::ptrdiff_t>(leftRead),
                        rightChunk.begin())) {
            identical = false;
        }
        remaining -= static_cast<quint64>(requested);
    }
    if (identical) {
        char leftExtra = 0;
        char rightExtra = 0;
        identical = readRetry(leftFd, &leftExtra, 1) == 0 &&
                    readRetry(rightFd, &rightExtra, 1) == 0;
    }
    static_cast<void>(::close(rightFd));
    static_cast<void>(::close(leftFd));
    return identical ? ContentComparison::Identical
                     : ContentComparison::DifferentOrUnverifiable;
}

// File-backed bounded region reader. The parser caps what it requests; this
// reader additionally refuses any region outside the stat-proven file size
// or any short read, so a shrinking or hostile file degrades to "absent".
IccRegionReader fileRegionReader(QFile &file, quint64 fileSize)
{
    return [&file, fileSize](quint32 offset, quint32 length) -> QByteArray {
        if (length == 0 || offset > fileSize || length > fileSize - offset) {
            return {};
        }
        if (!file.seek(static_cast<qint64>(offset))) {
            return {};
        }
        QByteArray data = file.read(static_cast<qint64>(length));
        if (data.size() != static_cast<qsizetype>(length)) {
            return {};
        }
        return data;
    };
}

struct ExaminedFile
{
    enum class Outcome
    {
        Profile,
        Skipped,
    };
    Outcome outcome = Outcome::Skipped;
    DiscoveredProfile profile;
    QList<DiscoveryDiagnostic> diagnostics;
};

ExaminedFile examineCandidateFile(const QString &fullPath, DiscoveryOrigin origin,
                                  const DiscoveryLimits &limits)
{
    ExaminedFile examined;
    const QFileInfo info(fullPath);
    if (info.isSymLink()) {
        examined.diagnostics.append(
            diagnostic(DiscoverySeverity::Warning, QStringLiteral("file-is-symlink"), fullPath));
        return examined;
    }
    if (!info.isFile()) {
        examined.diagnostics.append(
            diagnostic(DiscoverySeverity::Warning, QStringLiteral("not-regular-file"), fullPath));
        return examined;
    }
    const quint64 fileSize = static_cast<quint64>(info.size());
    if (fileSize < IccHeaderSizeBytes) {
        examined.diagnostics.append(
            diagnostic(DiscoverySeverity::Warning, QStringLiteral("file-too-small"), fullPath));
        return examined;
    }
    if (fileSize > MaxIccProfileSizeBytes) {
        examined.diagnostics.append(
            diagnostic(DiscoverySeverity::Warning, QStringLiteral("file-oversized"), fullPath));
        return examined;
    }

    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly)) {
        examined.diagnostics.append(
            diagnostic(DiscoverySeverity::Warning, QStringLiteral("file-unreadable"), fullPath));
        return examined;
    }
    const QByteArray headerBytes = file.read(IccHeaderSizeBytes);
    if (headerBytes.size() != static_cast<qsizetype>(IccHeaderSizeBytes)) {
        examined.diagnostics.append(
            diagnostic(DiscoverySeverity::Warning, QStringLiteral("file-truncated"), fullPath));
        return examined;
    }

    const auto [status, summary] = validateIccHeader(headerBytes, static_cast<quint32>(fileSize));
    if (status != ProfileValidationStatus::Valid) {
        examined.diagnostics.append(diagnostic(DiscoverySeverity::Warning,
                                               QStringLiteral("invalid-header"), fullPath,
                                               iccStatusDetail(status)));
        return examined;
    }
    // AGENT-GUARD: Exact declared-versus-actual size equality is the C0
    // descriptor consistency contract; a file whose header disagrees with
    // its real size is truncated or mislabeled provenance and must never be
    // published as import metadata.
    if (summary.profileSize != static_cast<quint32>(fileSize)) {
        examined.diagnostics.append(
            diagnostic(DiscoverySeverity::Warning, QStringLiteral("size-mismatch"), fullPath,
                       QStringLiteral("declared %1, actual %2")
                           .arg(summary.profileSize)
                           .arg(fileSize)));
        return examined;
    }

    quint32 tagCount = 0;
    if (fileSize >= IccHeaderSizeBytes + 4) {
        const QByteArray countBytes = fileRegionReader(file, fileSize)(IccHeaderSizeBytes, 4);
        if (countBytes.size() == 4) {
            tagCount = qFromBigEndian<quint32>(
                reinterpret_cast<const uchar *>(countBytes.constData()));
        }
    }

    const IccTextMetadata metadata = extractIccDescription(
        fileRegionReader(file, fileSize), tagCount, limits.maxTagTableEntries,
        limits.maxDescriptionTagBytes);

    const QString baseName = info.fileName();
    ExaminedFile profileResult;
    profileResult.outcome = ExaminedFile::Outcome::Profile;
    profileResult.profile.descriptor =
        assembleDescriptor(origin, baseName, headerBytes, static_cast<quint32>(fileSize),
                           metadata, QByteArray());
    profileResult.profile.sourcePath = fullPath;
    const ProfileValidationStatus descriptorStatus =
        validateProfileDescriptor(profileResult.profile.descriptor);
    if (descriptorStatus != ProfileValidationStatus::Valid) {
        examined.diagnostics.append(diagnostic(DiscoverySeverity::Warning,
                                               QStringLiteral("descriptor-invalid"), fullPath,
                                               iccStatusDetail(descriptorStatus)));
        return examined;
    }
    // Scan bounds degrade a still-catalogable profile with diagnostics rather
    // than growing reads without limit (ADR-0057).
    if (metadata.tagTableTruncated) {
        profileResult.diagnostics.append(
            diagnostic(DiscoverySeverity::Info, QStringLiteral("tag-table-truncated"), fullPath));
    }
    if (metadata.descriptionTagOversized) {
        profileResult.diagnostics.append(diagnostic(
            DiscoverySeverity::Info, QStringLiteral("description-tag-oversized"), fullPath));
    }
    return profileResult;
}

} // namespace

ProfileDiscovery::ProfileDiscovery(QList<DiscoveryRoot> roots, DiscoveryLimits limits)
    : m_roots(std::move(roots)), m_limits(limits)
{
}

bool ProfileDiscovery::limitsAreValid(const DiscoveryLimits &limits)
{
    return limits.maxFilesPerRoot >= 1 && limits.maxFilesPerRoot <= 65535 &&
           limits.maxTagTableEntries >= 1 && limits.maxTagTableEntries <= 1024 &&
           limits.maxDescriptionTagBytes >= 32 && limits.maxDescriptionTagBytes <= 65536;
}

DiscoveryResult ProfileDiscovery::discoverCatalog() const
{
    DiscoveryResult result;
    result.complete = true;
    if (!limitsAreValid(m_limits)) {
        result.complete = false;
        result.diagnostics.append(diagnostic(DiscoverySeverity::Warning,
                                             QStringLiteral("invalid-limits"), QString()));
        return result;
    }

    QList<DiscoveredProfile> examinedProfiles;
    for (const DiscoveryRoot &root : m_roots) {
        if (!validOrigin(root.origin)) {
            result.complete = false;
            result.diagnostics.append(diagnostic(DiscoverySeverity::Warning,
                                                 QStringLiteral("invalid-origin"), root.path));
            continue;
        }
        if (root.path.isEmpty()) {
            result.diagnostics.append(
                diagnostic(DiscoverySeverity::Warning, QStringLiteral("invalid-root"), root.path));
            continue;
        }
        const std::optional<CanonicalRootContainment> containment =
            CanonicalRootContainment::resolve(root.path);
        if (!containment.has_value()) {
            result.diagnostics.append(diagnostic(
                DiscoverySeverity::Warning, QStringLiteral("root-ancestor-is-symlink"), root.path));
            continue;
        }
        const QFileInfo rootInfo(containment->canonicalRoot());
        if (!rootInfo.isDir() || !rootInfo.isReadable()) {
            result.diagnostics.append(diagnostic(DiscoverySeverity::Info,
                                                 QStringLiteral("root-unavailable"), root.path));
            continue;
        }

        // The raw path is now proven to name the canonical root exactly. Keep
        // it for diagnostics/sourcePath compatibility while containment uses
        // only the retained canonical form.
        QDir directory(root.path);
        QStringList entries = directory.entryList(QDir::Files);
        std::sort(entries.begin(), entries.end());
        quint32 candidates = 0;
        for (const QString &entry : entries) {
            if (!hasDiscoverableIccExtension(entry)) {
                continue;
            }
            if (candidates >= m_limits.maxFilesPerRoot) {
                result.complete = false;
                result.diagnostics.append(
                    diagnostic(DiscoverySeverity::Warning,
                               QStringLiteral("root-file-budget-exceeded"), root.path));
                break;
            }
            ++candidates;
            const QString candidatePath = directory.filePath(entry);
            // AGENT-GUARD: Canonical containment is checked before QFileInfo,
            // open, or metadata parsing touches the candidate. A symlinked or
            // unresolvable entry is never discovery authority.
            if (!containment->containsExistingPath(candidatePath)) {
                result.diagnostics.append(diagnostic(
                    DiscoverySeverity::Warning, QStringLiteral("candidate-outside-root"),
                    candidatePath));
                continue;
            }
            ExaminedFile examined = examineCandidateFile(candidatePath, root.origin, m_limits);
            if (examined.outcome == ExaminedFile::Outcome::Profile) {
                examinedProfiles.append(examined.profile);
            }
            result.diagnostics.append(examined.diagnostics);
        }
    }

    // Conflict detection runs before C0 normalization purely so the
    // diagnostic records which deterministic identifier collided; the
    // normalization below performs the same order-independent rejection.
    QSet<QString> conflictedIds;
    QHash<QString, DiscoveredProfile> seenById;
    QList<IccProfileDescriptor> descriptors;
    descriptors.reserve(examinedProfiles.size());
    for (const DiscoveredProfile &profile : examinedProfiles) {
        const QString id = profile.descriptor.profileId;
        if (conflictedIds.contains(id)) {
            continue;
        }
        const auto seenIt = seenById.find(id);
        if (seenIt != seenById.end()) {
            // AGENT-GUARD: Equal inspected metadata is not proof of equal ICC
            // bytes. Compare colliding files in bounded chunks so a body-only
            // mutation cannot be collapsed as an exact duplicate (P1.3).
            if (seenIt->descriptor == profile.descriptor &&
                compareProfileContent(seenIt->sourcePath, profile.sourcePath) ==
                    ContentComparison::Identical) {
                result.diagnostics.append(
                    diagnostic(DiscoverySeverity::Info, QStringLiteral("duplicate-collapsed"),
                               profile.sourcePath));
                continue;
            }
            conflictedIds.insert(id);
            seenById.erase(seenIt);
            descriptors.removeIf([&id](const IccProfileDescriptor &d) { return d.profileId == id; });
            result.diagnostics.append(
                diagnostic(DiscoverySeverity::Warning, QStringLiteral("conflicting-profile-id"),
                           profile.sourcePath, id));
            continue;
        }
        seenById.insert(id, profile);
        descriptors.append(profile.descriptor);
    }

    if (descriptors.size() > static_cast<qsizetype>(MaxProfilesInCatalog)) {
        result.complete = false;
        result.diagnostics.append(diagnostic(DiscoverySeverity::Warning,
                                             QStringLiteral("catalog-cap-truncated"), QString()));
    }

    const QList<IccProfileDescriptor> normalized = normalizeAndSortCatalog(descriptors);
    QHash<QString, DiscoveredProfile> profileById;
    for (const DiscoveredProfile &profile : examinedProfiles) {
        // First occurrence in deterministic scan order wins; exact-equal
        // duplicates are interchangeable for lookup purposes.
        if (!profileById.contains(profile.descriptor.profileId)) {
            profileById.insert(profile.descriptor.profileId, profile);
        }
    }
    for (const IccProfileDescriptor &descriptor : normalized) {
        const auto it = profileById.find(descriptor.profileId);
        if (it != profileById.end() && it->descriptor == descriptor) {
            result.profiles.append(*it);
        }
    }
    return result;
}

ImportResult ProfileDiscovery::importUserProfile(const QString &sourcePath) const
{
    return importProfileFromSource(m_roots, m_limits, sourcePath);
}

} // namespace QindaQt::DisplayColor
