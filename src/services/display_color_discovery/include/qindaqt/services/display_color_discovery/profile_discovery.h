// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_color_model/color_types.h>
#include <qindaqt/services/display_color_model/color_validation.h>

#include <QtCore/QList>
#include <QtCore/QString>

namespace QindaQt::DisplayColor
{

// AGENT-CONTRACT: Discovery reads the host filesystem only through the roots
// the composition root injects here. The module never resolves HOME, XDG
// variables, or any default color directory itself, so private-session tests
// stay host-independent and production chooses its own search path. See the
// same rule for persistence roots in ADR-0051.
enum class DiscoveryOrigin : quint32
{
    BuiltIn = 0,
    System = 1,
    UserImported = 2,
};

struct DiscoveryRoot
{
    QString path;
    DiscoveryOrigin origin = DiscoveryOrigin::System;

    friend bool operator==(const DiscoveryRoot &, const DiscoveryRoot &) = default;
};

enum class DiscoverySeverity : quint32
{
    Info = 0,
    Warning = 1,
};

// One bounded, machine-stable diagnostic per skipped or degraded item. A
// hostile file degrades the catalog with diagnostics; it never aborts the
// scan or the process.
struct DiscoveryDiagnostic
{
    DiscoverySeverity severity = DiscoverySeverity::Info;
    QString code;
    QString path;
    QString detail;

    friend bool operator==(const DiscoveryDiagnostic &, const DiscoveryDiagnostic &) = default;
};

enum class ImportStatus : quint32
{
    Imported = 0,
    // The user root already holds a byte-identical copy under the same name;
    // nothing was written and the existing lineage is reported.
    AlreadyPresent = 1,
    InvalidSource = 2,
    SourceUnreadable = 3,
    SourceOversized = 4,
    SourceIsSymlink = 5,
    SourceNameUnsafe = 6,
    InvalidUserRoot = 7,
    DestinationConflict = 8,
    WriteFailed = 9,
    // The atomic replacement committed visibly but its directory durability
    // barrier failed; provenance is not claimed and a later identical import
    // converges on AlreadyPresent.
    DurabilityUncertain = 10,
};

// Scan bounds. All caps exist so a hostile or overflowing root costs bounded
// I/O and bounded memory; exceeding a bound degrades the catalog with a
// diagnostic instead of growing without limit.
struct DiscoveryLimits
{
    quint32 maxFilesPerRoot = 256;
    quint32 maxTagTableEntries = 64;
    quint32 maxDescriptionTagBytes = 1024;

    friend bool operator==(const DiscoveryLimits &, const DiscoveryLimits &) = default;
};

// Import metadata for one validated ICC file, ready for the C0 catalog
// pipeline (normalizeAndSortCatalog / ColorModel::setCatalog). Discovery
// reads only the 128-byte header plus a bounded description tag; it never
// interprets the profile body, so checksumSha256 stays empty for scanned
// profiles. A user import computes the SHA-256 content digest and stores it
// in checksumSha256 — that digest is the profile lineage fingerprint that
// assignment persistence records.
struct DiscoveredProfile
{
    IccProfileDescriptor descriptor;
    QString sourcePath;

    friend bool operator==(const DiscoveredProfile &, const DiscoveredProfile &) = default;
};

struct DiscoveryResult
{
    // C0-normalized: invalid descriptors filtered, exact duplicates
    // collapsed, sorted per the C0 order, capped at the catalog maximum.
    QList<DiscoveredProfile> profiles;
    QList<DiscoveryDiagnostic> diagnostics;
    // False when a scan bound (files per root, catalog cap) truncated the
    // enumeration; the published prefix is still deterministic.
    bool complete = true;

    friend bool operator==(const DiscoveryResult &, const DiscoveryResult &) = default;
};

struct ImportResult
{
    ImportStatus status = ImportStatus::InvalidSource;
    DiscoveredProfile profile;
    QString reasonCode;

    [[nodiscard]] bool imported() const noexcept
    {
        return status == ImportStatus::Imported || status == ImportStatus::AlreadyPresent;
    }

    friend bool operator==(const ImportResult &, const ImportResult &) = default;
};

// Synchronous, single-threaded ICC catalog discovery and user import over
// injected roots. No QObject, event loop, timer, or transport lives here;
// every call is independent and the object holds only injected
// configuration. Unreadable, truncated, oversized, mislabeled, or duplicate
// files produce diagnostics and are skipped; they never throw and never
// crash. The origin of a discovered profile is exactly the origin of the
// root it was found under.
class ProfileDiscovery final
{
public:
    ProfileDiscovery(QList<DiscoveryRoot> roots, DiscoveryLimits limits = {});

    // AGENT-GUARD: Limits must stay within sane bounds; a zero cap would let
    // a scan run unbounded or read unbounded tag payloads, so an invalid
    // configuration fails closed with an "invalid-limits" diagnostic instead
    // of scanning.
    [[nodiscard]] static bool limitsAreValid(const DiscoveryLimits &limits);

    [[nodiscard]] DiscoveryResult discoverCatalog() const;

    // Validates the source file, computes its SHA-256 lineage fingerprint,
    // and copies the exact bytes into the injected user root through a
    // same-directory atomic replace (temporary file created exclusively with
    // mode 0600, fsync, rename, directory barrier — the ADR-0051 pattern).
    // Every rejection happens before any user-root mutation; an interrupted
    // write leaves only a dot-prefixed temporary name that the next import
    // removes and discovery ignores.
    [[nodiscard]] ImportResult importUserProfile(const QString &sourcePath) const;

    [[nodiscard]] const QList<DiscoveryRoot> &roots() const noexcept { return m_roots; }
    [[nodiscard]] const DiscoveryLimits &limits() const noexcept { return m_limits; }

private:
    QList<DiscoveryRoot> m_roots;
    DiscoveryLimits m_limits;
};

} // namespace QindaQt::DisplayColor
