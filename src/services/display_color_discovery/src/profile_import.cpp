// SPDX-License-Identifier: LGPL-3.0-or-later

#include "profile_import_p.h"

#include "icc_text_metadata_p.h"
#include "import_writer_p.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QtEndian>

namespace QindaQt::DisplayColor
{
namespace
{

struct SourceCapture
{
    QByteArray content;
    quint64 fileSize = 0;
    ImportStatus failureStatus = ImportStatus::SourceUnreadable;
    QString failureReason;
    bool ok = false;
};

// Captures the exact source bytes under the declared bounds. Any size
// change under the open read is refused: provenance bytes must be captured
// completely before anything else happens.
SourceCapture captureSource(const QString &sourcePath)
{
    SourceCapture capture;
    const QFileInfo sourceInfo(sourcePath);
    if (sourceInfo.isSymLink()) {
        capture.failureStatus = ImportStatus::SourceIsSymlink;
        capture.failureReason = QStringLiteral("source-is-symlink");
        return capture;
    }
    if (!sourceInfo.isFile() || !sourceInfo.isReadable()) {
        capture.failureStatus = ImportStatus::SourceUnreadable;
        capture.failureReason = QStringLiteral("source-unreadable");
        return capture;
    }
    const quint64 fileSize = static_cast<quint64>(sourceInfo.size());
    if (fileSize > MaxIccProfileSizeBytes) {
        capture.failureStatus = ImportStatus::SourceOversized;
        capture.failureReason = QStringLiteral("source-oversized");
        return capture;
    }
    if (fileSize < IccHeaderSizeBytes) {
        capture.failureStatus = ImportStatus::InvalidSource;
        capture.failureReason = QStringLiteral("source-too-small");
        return capture;
    }
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        capture.failureStatus = ImportStatus::SourceUnreadable;
        capture.failureReason = QStringLiteral("source-unreadable");
        return capture;
    }
    QByteArray content = source.read(static_cast<qint64>(fileSize));
    char trailing = 0;
    const qint64 extra = source.read(&trailing, 1);
    if (content.size() != static_cast<qsizetype>(fileSize) || extra != 0) {
        capture.failureStatus = ImportStatus::SourceUnreadable;
        capture.failureReason = QStringLiteral("source-changed-during-read");
        return capture;
    }
    capture.content = std::move(content);
    capture.fileSize = fileSize;
    capture.ok = true;
    return capture;
}

} // namespace

ImportResult importProfileFromSource(const QList<DiscoveryRoot> &roots,
                                     const DiscoveryLimits &limits,
                                     const QString &sourcePath)
{
    ImportResult result;
    if (!ProfileDiscovery::limitsAreValid(limits)) {
        result.status = ImportStatus::InvalidSource;
        result.reasonCode = QStringLiteral("invalid-limits");
        return result;
    }

    const DiscoveryRoot *userRoot = nullptr;
    for (const DiscoveryRoot &root : roots) {
        if (root.origin == DiscoveryOrigin::UserImported) {
            userRoot = &root;
            break;
        }
    }
    if (userRoot == nullptr || userRoot->path.isEmpty()) {
        result.status = ImportStatus::InvalidUserRoot;
        result.reasonCode = QStringLiteral("no-user-root");
        return result;
    }

    const SourceCapture capture = captureSource(sourcePath);
    if (!capture.ok) {
        result.status = capture.failureStatus;
        result.reasonCode = capture.failureReason;
        return result;
    }

    const auto [status, summary] = validateIccHeader(
        QByteArray::fromRawData(capture.content.constData(),
                                static_cast<qsizetype>(IccHeaderSizeBytes)),
        static_cast<quint32>(capture.fileSize));
    if (status != ProfileValidationStatus::Valid ||
        summary.profileSize != static_cast<quint32>(capture.fileSize)) {
        result.status = ImportStatus::InvalidSource;
        result.reasonCode =
            status != ProfileValidationStatus::Valid
                ? QStringLiteral("invalid-icc-header/") + iccStatusDetail(status)
                : QStringLiteral("size-mismatch");
        return result;
    }

    const QFileInfo sourceInfo(sourcePath);
    const QString baseName = sourceInfo.fileName();
    if (sanitizeProfileIdFromFileName(iccFileStem(baseName)).isEmpty() ||
        !destinationNameIsSafe(baseName) || !hasDiscoverableIccExtension(baseName)) {
        result.status = ImportStatus::SourceNameUnsafe;
        result.reasonCode = QStringLiteral("source-name-unsafe");
        return result;
    }

    // AGENT-GUARD: The destination file name becomes the stored profile's
    // public fileName and the identity stem's source. It must pass the C0
    // safety grammar so a hostile name can never escape the user root
    // through persistence; the descriptor validation below stays the
    // authority and this check only refines the rejection code.
    const quint32 tagCount = [&content = capture.content] {
        const QByteArray countBytes = memoryRegionReader(content)(IccHeaderSizeBytes, 4);
        if (countBytes.size() != 4) {
            return quint32{0};
        }
        return qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(countBytes.constData()));
    }();
    const IccTextMetadata metadata =
        extractIccDescription(memoryRegionReader(capture.content), tagCount,
                              limits.maxTagTableEntries, limits.maxDescriptionTagBytes);

    const QByteArray lineageFingerprint =
        QCryptographicHash::hash(capture.content, QCryptographicHash::Sha256);

    DiscoveredProfile imported;
    imported.descriptor = assembleDescriptor(
        DiscoveryOrigin::UserImported, baseName,
        QByteArray::fromRawData(capture.content.constData(), IccHeaderSizeBytes),
        static_cast<quint32>(capture.fileSize), metadata, lineageFingerprint);
    imported.sourcePath = sourcePath;
    const ProfileValidationStatus importedStatus = validateProfileDescriptor(imported.descriptor);
    if (importedStatus != ProfileValidationStatus::Valid) {
        result.status = ImportStatus::InvalidSource;
        result.reasonCode = QStringLiteral("invalid-metadata/") + iccStatusDetail(importedStatus);
        return result;
    }

    const std::optional<QByteArray> existingDigest =
        existingFileDigestIfIdentical(userRoot->path, baseName, capture.content);
    if (existingDigest.has_value()) {
        result.status = ImportStatus::AlreadyPresent;
        result.profile = imported;
        return result;
    }
    const QFileInfo destinationInfo(QDir(userRoot->path).filePath(baseName));
    if (destinationInfo.exists() || destinationInfo.isSymLink()) {
        result.status = ImportStatus::DestinationConflict;
        result.reasonCode = QStringLiteral("destination-conflict");
        return result;
    }

    const ImportWriteOutcome written =
        atomicWriteProfileCopy(userRoot->path, baseName, capture.content);
    if (written.status != ImportWriteOutcome::Status::Written) {
        result.status = written.reasonCode == QStringLiteral("durability-uncertain")
                            ? ImportStatus::DurabilityUncertain
                            : ImportStatus::WriteFailed;
        result.reasonCode = written.reasonCode;
        return result;
    }

    result.status = ImportStatus::Imported;
    result.profile = imported;
    return result;
}

} // namespace QindaQt::DisplayColor
