// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <optional>

namespace QindaQt::DisplayColor
{

// Reason codes reported by the atomic import writer. They are stable
// diagnostics, not localizable text.
inline constexpr auto ImportWriteFailureCode = "write-failed";
inline constexpr auto ImportRootUnsafeCode = "invalid-user-root";
inline constexpr auto ImportDestinationConflictCode = "destination-conflict";

struct ImportWriteOutcome
{
    enum class Status
    {
        Written,
        Failed,
    };
    Status status = Status::Failed;
    QString reasonCode;
};

// AGENT-CONTRACT: The user root receives profiles through same-directory
// atomic replacement only, mirroring ADR-0051's durability pattern: the root
// must already exist as a non-symlink, effective-user-owned directory that is
// not group- or other-writable; the temporary name is created exclusively
// with mode 0600; the payload is fsynced; the rename is the commit point;
// and the directory barrier is applied where the filesystem supports it.
// Callers must serialize imports into one root (single-threaded ownership).
// The destination name is a caller-validated C0-safe file base name; no
// path separator or ".." can reach this layer.
ImportWriteOutcome atomicWriteProfileCopy(const QString &userRoot, const QString &fileName,
                                          const QByteArray &content);

// Returns the stored file's SHA-256 content digest when the destination is a
// regular, non-symlink file that is byte-identical to the supplied content;
// nullopt when it is absent, unreadable, or different.
std::optional<QByteArray> existingFileDigestIfIdentical(const QString &root, const QString &fileName,
                                                        const QByteArray &content);

} // namespace QindaQt::DisplayColor
