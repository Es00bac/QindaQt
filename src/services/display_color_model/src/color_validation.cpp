// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/display_color_model/color_validation.h"
#include "qindaqt/services/display_color_model/color_limits.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace QindaQt::DisplayColor
{

namespace
{

// AGENT-GUARD: ICC profile/device classes, data color spaces, and PCS spaces
// are fixed four-byte registered signatures; anything else is hostile or
// corrupt data and must fail closed rather than be interpreted.
bool isSupportedDeviceClass(const QByteArray &tag)
{
    // Standard ICC profile/device classes:
    // 'mntr': Display device profile
    // 'scnr': Input device profile
    // 'prtr': Output device profile
    // 'link': DeviceLink profile
    // 'spac': ColorSpace conversion profile
    // 'abst': Abstract profile
    // 'nmcl': NamedColor profile
    return tag == "mntr" || tag == "spac" || tag == "scnr" || tag == "prtr" ||
           tag == "link" || tag == "abst" || tag == "nmcl";
}

bool isSupportedDataColorSpace(const QByteArray &tag)
{
    // AGENT-CONTRACT: Display Color C0 requires RGB or monochrome/gray space
    // descriptors for display device/space profiles. Other spaces (CMYK, Lab)
    // are rejected as invalid for display output assignment.
    return tag == "RGB " || tag == "GRAY";
}

bool isSupportedConnectionSpace(const QByteArray &tag)
{
    // ICC PCS must be XYZ or Lab
    return tag == "XYZ " || tag == "Lab ";
}

bool isValidIdentifierString(const QString &id)
{
    if (id.isEmpty() || id.size() > MaxIdentifierLength) {
        return false;
    }
    // AGENT-GUARD: the durable identifier grammar is exactly
    // [A-Za-z0-9._:-]. Unicode letters/digits (e.g. 'écran') are outside
    // the documented ASCII grammar and must fail closed so identifiers stay
    // byte-stable across every consumer lane.
    for (const QChar ch : id) {
        const ushort u = ch.unicode();
        const bool asciiAlnum = (u >= u'a' && u <= u'z') ||
                                (u >= u'A' && u <= u'Z') ||
                                (u >= u'0' && u <= u'9');
        if (!asciiAlnum && u != u'.' && u != u':' && u != u'-' && u != u'_') {
            return false;
        }
    }
    return true;
}

bool isValidFilename(const QString &filename)
{
    if (filename.isEmpty()) {
        return true; // Optional
    }
    if (filename.size() > MaxFilenameLength) {
        return false;
    }
    // Disallow path separators, control characters, and parent directory traversal
    if (filename.contains(u'/') || filename.contains(u'\\') || filename.contains("..")) {
        return false;
    }
    for (const QChar ch : filename) {
        if (ch.isSpace() || ch.unicode() < 0x20 || ch.unicode() == 0x7F) {
            return false;
        }
    }
    return true;
}

// AGENT-GUARD: Hostile storage can cast arbitrary quint32 values into these
// enums. Every enum crossing the public validation boundary is range checked
// against its fixed enumerators so an unknown value never becomes catalog,
// capability, or assignment truth.
bool isValidProfileOriginValue(quint32 value)
{
    return value >= static_cast<quint32>(ProfileOrigin::BuiltIn) &&
           value <= static_cast<quint32>(ProfileOrigin::EdidDerived);
}

bool isValidGamutValue(quint32 value)
{
    return value >= static_cast<quint32>(ColorSpaceGamut::Srgb) &&
           value <= static_cast<quint32>(ColorSpaceGamut::Custom);
}

bool isValidTransferFunctionValue(quint32 value)
{
    return value >= static_cast<quint32>(TransferFunction::Srgb) &&
           value <= static_cast<quint32>(TransferFunction::Gamma22);
}

bool isValidPolicyValue(quint32 value)
{
    return value >= static_cast<quint32>(OutputColorPolicy::SdrSrgb) &&
           value <= static_cast<quint32>(OutputColorPolicy::AutoColorManagement);
}

bool isValidIntentValue(quint32 value)
{
    return value >= static_cast<quint32>(RenderingIntent::Perceptual) &&
           value <= static_cast<quint32>(RenderingIntent::AbsoluteColorimetric);
}

// AGENT-CONTRACT: The lineage fingerprint encoding frames every field as
// [u16 tag length][tag][u64 payload length][payload]. The frame is fully
// self-delimiting, so two distinct field tuples can never produce the same
// byte stream: moving payload bytes between adjacent fields (for example
// default="a"+profile="bc" versus default="ab"+profile="c") changes the
// framed stream, where an unframed concatenation would collide on "abc".
// Tags are fixed ASCII domain identifiers; the schema tag versions the
// encoding itself so a future field set can never alias this one.

void frameField(QCryptographicHash &hash, QByteArrayView tag, QByteArrayView payload)
{
    const quint16 tagLength = qToBigEndian(static_cast<quint16>(tag.size()));
    hash.addData(QByteArrayView(reinterpret_cast<const char *>(&tagLength), sizeof(tagLength)));
    hash.addData(tag);
    const quint64 payloadLength = qToBigEndian(static_cast<quint64>(payload.size()));
    hash.addData(QByteArrayView(reinterpret_cast<const char *>(&payloadLength), sizeof(payloadLength)));
    hash.addData(payload);
}

void frameU32(QCryptographicHash &hash, QByteArrayView tag, quint32 value)
{
    const quint32 be = qToBigEndian(value);
    frameField(hash, tag, QByteArrayView(reinterpret_cast<const char *>(&be), sizeof(be)));
}

void frameU64(QCryptographicHash &hash, QByteArrayView tag, quint64 value)
{
    const quint64 be = qToBigEndian(value);
    frameField(hash, tag, QByteArrayView(reinterpret_cast<const char *>(&be), sizeof(be)));
}

void frameDouble(QCryptographicHash &hash, QByteArrayView tag, double value)
{
    // IEEE-754 bit pattern framed as u64 keeps the encoding canonical across
    // platforms; NaN payloads are irrelevant here because published values
    // were already validated finite.
    quint64 bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "double must be 64-bit");
    std::memcpy(&bits, &value, sizeof(bits));
    frameU64(hash, tag, bits);
}

void frameBool(QCryptographicHash &hash, QByteArrayView tag, bool value)
{
    const char byte = value ? '\x01' : '\x00';
    frameField(hash, tag, QByteArrayView(&byte, 1));
}

void frameString(QCryptographicHash &hash, QByteArrayView tag, const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    frameField(hash, tag, utf8);
}

void frameAssignmentFields(QCryptographicHash &hash, const OutputColorAssignment &assignment)
{
    frameString(hash, "a.id", assignment.stableId);
    frameString(hash, "a.profile", assignment.profileId);
    frameU32(hash, "a.policy", static_cast<quint32>(assignment.policy));
    frameU32(hash, "a.intent", static_cast<quint32>(assignment.intent));
    frameBool(hash, "a.sdr-gain", assignment.sdrBrightnessGainApplied);
    frameDouble(hash, "a.sdr-nits", assignment.sdrBrightnessNits);
    frameBool(hash, "a.wire", assignment.wireValid);
}

void frameCapabilitiesFields(QCryptographicHash &hash, const OutputColorCapabilities &capabilities)
{
    frameString(hash, "cap.id", capabilities.stableId);
    frameBool(hash, "cap.wcg", capabilities.supportsWcg);
    frameBool(hash, "cap.hdr", capabilities.supportsHdr);
    frameU32(hash, "cap.gamut-count", static_cast<quint32>(capabilities.supportedGamuts.size()));
    for (const ColorSpaceGamut gamut : capabilities.supportedGamuts) {
        frameU32(hash, "cap.gamut", static_cast<quint32>(gamut));
    }
    frameU32(hash, "cap.transfer-count", static_cast<quint32>(capabilities.supportedTransferFunctions.size()));
    for (const TransferFunction transfer : capabilities.supportedTransferFunctions) {
        frameU32(hash, "cap.transfer", static_cast<quint32>(transfer));
    }
    frameDouble(hash, "cap.min-nits", capabilities.minLuminanceNits);
    frameDouble(hash, "cap.max-nits", capabilities.maxLuminanceNits);
    frameDouble(hash, "cap.full-frame-nits", capabilities.maxFullFrameLuminanceNits);
    frameBool(hash, "cap.auto-acm", capabilities.autoColorManagementAvailable);
    frameBool(hash, "cap.wire", capabilities.wireValid);
}

} // namespace

std::pair<ProfileValidationStatus, IccHeaderSummary> validateIccHeader(
    const QByteArray &headerData,
    quint32 totalFileSize)
{
    IccHeaderSummary summary;

    if (headerData.isEmpty()) {
        return {ProfileValidationStatus::EmptyData, summary};
    }

    if (headerData.size() < static_cast<qsizetype>(IccHeaderSizeBytes)) {
        return {ProfileValidationStatus::HeaderTooSmall, summary};
    }

    const uchar *bytes = reinterpret_cast<const uchar *>(headerData.constData());

    // 0..3: Profile size (big endian)
    const quint32 declaredSize = qFromBigEndian<quint32>(bytes);
    summary.profileSize = declaredSize;

    if (declaredSize < MinIccProfileSizeBytes || declaredSize > MaxIccProfileSizeBytes) {
        return {ProfileValidationStatus::InvalidDeclaredSize, summary};
    }

    if (totalFileSize > 0 && declaredSize > totalFileSize) {
        return {ProfileValidationStatus::InvalidDeclaredSize, summary};
    }

    // AGENT-GUARD: The supplied bytes cannot exceed the profile's own
    // declared size; a header buffer larger than the profile it claims to
    // come from is inconsistent hostile input, not a truncation. The
    // declared size was already bounded by any consistent total file size
    // above, so this bound also subsumes the total-size comparison.
    if (headerData.size() > static_cast<qsizetype>(declaredSize)) {
        return {ProfileValidationStatus::InvalidSize, summary};
    }

    // 4..7: CMM type
    summary.cmmType = qFromBigEndian<quint32>(bytes + 4);

    // 8..11: Version. The major version byte must name a known ICC spec
    // generation (2 through 5); version 1 predates the published spec and an
    // unknown future major cannot be interpreted safely.
    summary.version = qFromBigEndian<quint32>(bytes + 8);
    const quint32 majorVersion = (summary.version >> 24) & 0xFFu;
    if (majorVersion < 2 || majorVersion > 5) {
        return {ProfileValidationStatus::InvalidVersion, summary};
    }

    // 12..15: Device class
    summary.deviceClass = headerData.mid(12, 4);
    if (!isSupportedDeviceClass(summary.deviceClass)) {
        return {ProfileValidationStatus::UnsupportedProfileClass, summary};
    }

    // 16..19: Data color space
    summary.dataColorSpace = headerData.mid(16, 4);
    if (!isSupportedDataColorSpace(summary.dataColorSpace)) {
        return {ProfileValidationStatus::UnsupportedColorSpace, summary};
    }

    // 20..23: Connection space
    summary.connectionSpace = headerData.mid(20, 4);
    if (!isSupportedConnectionSpace(summary.connectionSpace)) {
        return {ProfileValidationStatus::UnsupportedConnectionSpace, summary};
    }

    // 36..39: Magic signature 'acsp'
    const quint32 magic = qFromBigEndian<quint32>(bytes + 36);
    if (magic != IccMagicAcsp) {
        return {ProfileValidationStatus::InvalidMagic, summary};
    }

    // 84..99: Profile ID (16 bytes MD5)
    summary.profileIdMd5 = headerData.mid(84, 16);

    summary.valid = true;
    return {ProfileValidationStatus::Valid, summary};
}

ProfileValidationStatus validateProfileDescriptor(const IccProfileDescriptor &descriptor)
{
    if (!descriptor.wireValid) {
        return ProfileValidationStatus::MalformedMetadata;
    }

    if (!isValidIdentifierString(descriptor.profileId)) {
        return ProfileValidationStatus::MalformedMetadata;
    }

    if (descriptor.displayName.trimmed().isEmpty() ||
        descriptor.displayName.size() > MaxDisplayNameLength) {
        return ProfileValidationStatus::MalformedMetadata;
    }

    if (descriptor.description.size() > MaxDescriptionLength) {
        return ProfileValidationStatus::MalformedMetadata;
    }

    if (!isValidFilename(descriptor.fileName)) {
        return ProfileValidationStatus::MalformedMetadata;
    }

    if (!isValidProfileOriginValue(static_cast<quint32>(descriptor.origin)) ||
        !isValidGamutValue(static_cast<quint32>(descriptor.gamut)) ||
        !isValidTransferFunctionValue(static_cast<quint32>(descriptor.transferFunction))) {
        return ProfileValidationStatus::MalformedMetadata;
    }

    if (descriptor.byteSize < MinIccProfileSizeBytes ||
        descriptor.byteSize > MaxIccProfileSizeBytes) {
        return ProfileValidationStatus::InvalidSize;
    }

    if (!descriptor.checksumSha256.isEmpty() && descriptor.checksumSha256.size() != 32) {
        return ProfileValidationStatus::ChecksumMismatch;
    }

    const auto [status, summary] = validateIccHeader(descriptor.rawHeader, descriptor.byteSize);
    if (status != ProfileValidationStatus::Valid) {
        return status;
    }

    // AGENT-GUARD: The descriptor's declared byte size and the header's own
    // declared profile size must match exactly. A descriptor size smaller or
    // larger than the embedded declaration is inconsistent provenance for
    // the same bytes and must fail closed.
    if (summary.profileSize != descriptor.byteSize) {
        return ProfileValidationStatus::InvalidSize;
    }

    return ProfileValidationStatus::Valid;
}

bool validateDisplayStableId(const QString &stableId)
{
    return isValidIdentifierString(stableId);
}

bool validateOutputCapabilities(const OutputColorCapabilities &capabilities)
{
    if (!capabilities.wireValid) {
        return false;
    }

    if (!validateDisplayStableId(capabilities.stableId)) {
        return false;
    }

    // AGENT-GUARD: NaN luminance passes every ordered comparison, so finite
    // checks must precede range checks; otherwise a hostile NaN capability
    // would be accepted and poison every snapshot derived from it.
    if (!std::isfinite(capabilities.minLuminanceNits) ||
        !std::isfinite(capabilities.maxLuminanceNits) ||
        !std::isfinite(capabilities.maxFullFrameLuminanceNits)) {
        return false;
    }

    if (capabilities.minLuminanceNits < MinLuminanceNits ||
        capabilities.maxLuminanceNits > MaxLuminanceNits ||
        capabilities.minLuminanceNits > capabilities.maxLuminanceNits ||
        capabilities.maxFullFrameLuminanceNits < capabilities.minLuminanceNits ||
        capabilities.maxFullFrameLuminanceNits > capabilities.maxLuminanceNits) {
        return false;
    }

    // The gamut/transfer lists are bounded by their fixed enumerator counts so
    // a hostile aggregate cannot carry unbounded or unknown entries.
    if (capabilities.supportedGamuts.size() > 5 ||
        capabilities.supportedTransferFunctions.size() > 5) {
        return false;
    }
    for (const ColorSpaceGamut gamut : capabilities.supportedGamuts) {
        if (!isValidGamutValue(static_cast<quint32>(gamut))) {
            return false;
        }
    }
    for (const TransferFunction transfer : capabilities.supportedTransferFunctions) {
        if (!isValidTransferFunctionValue(static_cast<quint32>(transfer))) {
            return false;
        }
    }

    // AGENT-GUARD: If HDR capability is claimed, the display must support at least
    // one high-dynamic-range transfer function (PQ or HLG).
    if (capabilities.supportsHdr) {
        const bool hasHdrTransfer = capabilities.supportedTransferFunctions.contains(TransferFunction::Pq) ||
                                    capabilities.supportedTransferFunctions.contains(TransferFunction::Hlg);
        if (!hasHdrTransfer) {
            return false;
        }
    }

    return true;
}

bool validateOutputAssignment(const OutputColorAssignment &assignment)
{
    if (!assignment.wireValid) {
        return false;
    }

    if (!validateDisplayStableId(assignment.stableId)) {
        return false;
    }

    if (!isValidPolicyValue(static_cast<quint32>(assignment.policy)) ||
        !isValidIntentValue(static_cast<quint32>(assignment.intent))) {
        return false;
    }

    if (!assignment.profileId.isEmpty() && !isValidIdentifierString(assignment.profileId)) {
        return false;
    }

    if (!std::isfinite(assignment.sdrBrightnessNits) ||
        assignment.sdrBrightnessNits < MinLuminanceNits ||
        assignment.sdrBrightnessNits > MaxLuminanceNits) {
        return false;
    }

    return true;
}

QList<IccProfileDescriptor> normalizeAndSortCatalog(const QList<IccProfileDescriptor> &profiles)
{
    QHash<QString, IccProfileDescriptor> acceptedById;
    QSet<QString> conflictedIds;

    for (const auto &profile : profiles) {
        if (validateProfileDescriptor(profile) != ProfileValidationStatus::Valid) {
            continue;
        }

        if (conflictedIds.contains(profile.profileId)) {
            continue;
        }

        const auto existingIt = acceptedById.find(profile.profileId);
        if (existingIt != acceptedById.end()) {
            // AGENT-GUARD: duplicate resolution must be independent of input
            // order. Exact-equal duplicates collapse to one entry; a
            // conflicting pair (same ID, different bytes) is rejected
            // atomically — neither descriptor becomes catalog truth — so
            // two conflicting descriptors can never publish different
            // catalogs depending on which arrived first.
            if (!(*existingIt == profile)) {
                acceptedById.erase(existingIt);
                conflictedIds.insert(profile.profileId);
            }
            continue;
        }

        acceptedById.insert(profile.profileId, profile);
    }

    QList<IccProfileDescriptor> validProfiles = acceptedById.values();

    // AGENT-CONTRACT: Deterministic sorting order ensures catalog representations
    // produce identical serialized snapshots across processes and restarts:
    // 1. Origin (BuiltIn < System < UserImported < EdidDerived)
    // 2. Case-insensitive DisplayName
    // 3. Exact ProfileId
    // The capacity cap is applied after sorting so even an oversized input
    // set publishes the same deterministic prefix in any input order.
    std::sort(validProfiles.begin(), validProfiles.end(), [](const IccProfileDescriptor &a, const IccProfileDescriptor &b) {
        if (a.origin != b.origin) {
            return static_cast<quint32>(a.origin) < static_cast<quint32>(b.origin);
        }
        const int nameCmp = QString::compare(a.displayName, b.displayName, Qt::CaseInsensitive);
        if (nameCmp != 0) {
            return nameCmp < 0;
        }
        return a.profileId < b.profileId;
    });

    if (validProfiles.size() > static_cast<int>(MaxProfilesInCatalog)) {
        validProfiles.resize(static_cast<int>(MaxProfilesInCatalog));
    }

    return validProfiles;
}

QByteArray computeLineageFingerprint(
    const QString &serviceEpoch,
    quint64 revision,
    const ColorCatalog &catalog,
    const QList<OutputColorState> &outputs)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);

    // AGENT-GUARD: the fingerprint must cover every semantically published
    // snapshot field. Catalog profiles are framed in the caller's sorted
    // catalog order; output states are sorted by stable ID here so the
    // encoding is deterministic. Omitting any published field (display
    // name, gamut, capabilities, requested assignment, flags, ...) would
    // let two different snapshots share one lineage fingerprint, so every
    // added published field must also be framed and pinned by a mutation
    // regression.

    frameU32(hash, "schema", 1);
    frameString(hash, "epoch", serviceEpoch);
    frameU64(hash, "revision", revision);
    frameString(hash, "catalog.default", catalog.defaultSrgbProfileId);
    frameBool(hash, "catalog.wire", catalog.wireValid);
    frameU32(hash, "catalog.profile-count", static_cast<quint32>(catalog.profiles.size()));
    for (const auto &profile : catalog.profiles) {
        frameString(hash, "prof.id", profile.profileId);
        frameString(hash, "prof.name", profile.displayName);
        frameString(hash, "prof.description", profile.description);
        frameString(hash, "prof.file", profile.fileName);
        frameU32(hash, "prof.origin", static_cast<quint32>(profile.origin));
        frameU32(hash, "prof.gamut", static_cast<quint32>(profile.gamut));
        frameU32(hash, "prof.transfer", static_cast<quint32>(profile.transferFunction));
        frameField(hash, "prof.header", profile.rawHeader);
        frameField(hash, "prof.checksum", profile.checksumSha256);
        frameU32(hash, "prof.byte-size", profile.byteSize);
        frameBool(hash, "prof.wire", profile.wireValid);
    }

    // stable_sort keeps equal-ID entries in input order, so even a hostile
    // caller-supplied list with duplicate stable IDs fingerprints
    // deterministically.
    QList<OutputColorState> sortedOutputs = outputs;
    std::stable_sort(sortedOutputs.begin(), sortedOutputs.end(), [](const OutputColorState &a, const OutputColorState &b) {
        return a.stableId < b.stableId;
    });

    frameU32(hash, "outputs.count", static_cast<quint32>(sortedOutputs.size()));
    for (const auto &out : sortedOutputs) {
        frameString(hash, "out.id", out.stableId);
        frameString(hash, "out.role", "capabilities");
        frameCapabilitiesFields(hash, out.capabilities);
        frameString(hash, "out.role", "requested");
        frameAssignmentFields(hash, out.requestedAssignment);
        frameString(hash, "out.role", "applied");
        frameAssignmentFields(hash, out.appliedAssignment);
        frameString(hash, "out.active-profile", out.activeProfileId);
        frameU32(hash, "out.degraded-reason", static_cast<quint32>(out.degradedReason));
        frameBool(hash, "out.is-degraded", out.isDegraded);
        frameBool(hash, "out.wire", out.wireValid);
    }

    return hash.result();
}

} // namespace QindaQt::DisplayColor
