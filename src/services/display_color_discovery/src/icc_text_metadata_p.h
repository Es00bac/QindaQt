// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <qindaqt/services/display_color_discovery/profile_discovery.h>

#include <functional>

namespace QindaQt::DisplayColor
{

// Bounded reader for one region of an ICC file. Discovery supplies a reader
// that pulls from the open file with per-call caps; import slices an
// in-memory buffer. Implementations must return an empty byte array for any
// region they refuse to serve; the parser treats that as "absent".
using IccRegionReader = std::function<QByteArray(quint32 offset, quint32 length)>;

struct IccTextMetadata
{
    QString description;
    bool descriptionValid = false;
    // True when the file's tag table had more entries than the scan bound;
    // the description search ran over the truncated prefix only.
    bool tagTableTruncated = false;
    // True when a description tag ('desc'/'mluc') exceeded the bounded tag
    // byte budget and was skipped rather than read.
    bool descriptionTagOversized = false;
};

// Extracts the human-readable description from the ICC tag table using only
// bounded regions. Understands the two registered description encodings —
// v2 'desc' textDescriptionType and v4 'mluc' multiLocalizedUnicode — and
// fails soft: any hostile offset, length, count, or encoding yields an
// absent description rather than an error, because a profile without a
// parsable description is still catalogable under its file-derived name.
// AGENT-GUARD: Never parse profile body bytes outside the regions this
// function requests; unbounded body interpretation is the import-lane
// security boundary this module exists to keep.
IccTextMetadata extractIccDescription(const IccRegionReader &readRegion,
                                      quint32 tagTableEntries,
                                      quint32 tagTableEntriesCap,
                                      quint32 descriptionTagBytesCap);

// Deterministic display name for a discovered profile: the first non-blank
// line of a valid description when it fits the C0 display-name bound,
// otherwise the caller-supplied file-derived fallback (already non-blank).
QString chooseDiscoveredDisplayName(const IccTextMetadata &metadata, const QString &fallbackName);

// Maps a file base name to the C0 identifier grammar ([A-Za-z0-9._:-]) by
// dropping every other character. Returns an empty string when nothing
// survives or the result exceeds the identifier bound; callers skip such
// files with a diagnostic.
QString sanitizeProfileIdFromFileName(const QString &fileName);

// Stable diagnostic text for a C0 validation status; owned here because the
// read-only C0 model exposes no status names.
QString iccStatusDetail(ProfileValidationStatus status);

// The file base name without its final extension (used for profile identity).
QString iccFileStem(const QString &fileName);

// True exactly for the suffixes discovery enumerates. Import must share this
// predicate so every successful stored name can re-enter the catalog.
bool hasDiscoverableIccExtension(const QString &fileName);

// Region reader over an in-memory buffer; slices or returns empty.
IccRegionReader memoryRegionReader(const QByteArray &content);

// Assembles the C0 descriptor for one validated ICC file. AGENT-GUARD:
// discovered/imported profiles carry unproven color semantics (Custom gamut,
// non-sRGB transfer) so they can never satisfy the C0 hasSrgbSemantics rule
// before a consumer classifies real semantics.
IccProfileDescriptor assembleDescriptor(DiscoveryOrigin origin, const QString &fileName,
                                        const QByteArray &headerBytes, quint32 fileSize,
                                        const IccTextMetadata &metadata,
                                        const QByteArray &lineageFingerprint);

// Same rules the C0 descriptor validator enforces for fileName; used for the
// precise import rejection code. The descriptor validator stays the
// authority.
bool destinationNameIsSafe(const QString &fileName);

} // namespace QindaQt::DisplayColor
