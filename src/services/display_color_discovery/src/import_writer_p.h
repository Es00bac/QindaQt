// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "path_safety_p.h"

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <optional>

namespace QindaQt::DisplayColor
{

inline constexpr auto ImportWriteFailureCode = "write-failed";
inline constexpr auto ImportRootUnsafeCode = "invalid-user-root";

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

struct ExistingDestinationOutcome
{
    enum class Status
    {
        Missing,
        Identical,
        Conflict,
    };
    Status status = Status::Conflict;
    QByteArray digest;
};

// AGENT-CONTRACT: One instance owns the canonicalized and descriptor-open
// import root for one import operation. Creation rejects parent references,
// canonicalization failures, symlinked components, foreign ownership, and
// group/other writability before any destination is inspected. Callers must
// then pass every destination through destinationIsContained before stat/read.
class ImportRootAccess final
{
public:
    static std::optional<ImportRootAccess> open(const QString &injectedRoot);

    ~ImportRootAccess();
    ImportRootAccess(const ImportRootAccess &) = delete;
    ImportRootAccess &operator=(const ImportRootAccess &) = delete;
    ImportRootAccess(ImportRootAccess &&other) noexcept;
    ImportRootAccess &operator=(ImportRootAccess &&other) noexcept;

    [[nodiscard]] bool destinationIsContained(const QString &fileName) const;
    [[nodiscard]] ExistingDestinationOutcome inspectExisting(
        const QString &fileName, const QByteArray &content) const;
    [[nodiscard]] ImportWriteOutcome write(const QString &fileName,
                                            const QByteArray &content) const;

private:
    ImportRootAccess(CanonicalRootContainment containment, int fd);

    [[nodiscard]] QString destinationPath(const QString &fileName) const;

    CanonicalRootContainment m_containment;
    int m_fd = -1;
};

} // namespace QindaQt::DisplayColor
