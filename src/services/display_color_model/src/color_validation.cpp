// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/display_color_model/color_validation.h"
#include "qindaqt/services/display_color_model/color_limits.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QSet>
#include <QtCore/QtEndian>
#include <algorithm>
#include <cmath>

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
    for (const QChar ch : id) {
        if (!ch.isLetterOrNumber() && ch != u'-' && ch != u'_' && ch != u'.' && ch != u':') {
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

    // AGENT-GUARD: The supplied bytes cannot exceed the declared total profile
    // size; a header buffer larger than the profile it claims to come from is
    // inconsistent hostile input, not a truncation.
    if (totalFileSize > 0 && headerData.size() > static_cast<qsizetype>(totalFileSize)) {
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
    QList<IccProfileDescriptor> validProfiles;
    validProfiles.reserve(std::min<qsizetype>(profiles.size(), static_cast<qsizetype>(MaxProfilesInCatalog)));

    QSet<QString> seenIds;

    for (const auto &profile : profiles) {
        if (validProfiles.size() >= static_cast<int>(MaxProfilesInCatalog)) {
            break;
        }

        if (validateProfileDescriptor(profile) != ProfileValidationStatus::Valid) {
            continue;
        }

        if (seenIds.contains(profile.profileId)) {
            continue;
        }

        seenIds.insert(profile.profileId);
        validProfiles.append(profile);
    }

    // AGENT-CONTRACT: Deterministic sorting order ensures catalog representations
    // produce identical serialized snapshots across processes and restarts:
    // 1. Origin (BuiltIn < System < UserImported < EdidDerived)
    // 2. Case-insensitive DisplayName
    // 3. Exact ProfileId
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

    return validProfiles;
}

QByteArray computeLineageFingerprint(
    const QString &serviceEpoch,
    quint64 revision,
    const ColorCatalog &catalog,
    const QList<OutputColorState> &outputs)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);

    hash.addData(serviceEpoch.toUtf8());
    const quint64 revBe = qToBigEndian(revision);
    hash.addData(QByteArrayView(reinterpret_cast<const char *>(&revBe), sizeof(revBe)));
    hash.addData(catalog.defaultSrgbProfileId.toUtf8());

    for (const auto &profile : catalog.profiles) {
        hash.addData(profile.profileId.toUtf8());
        hash.addData(profile.checksumSha256);
        const quint32 originVal = qToBigEndian(static_cast<quint32>(profile.origin));
        hash.addData(QByteArrayView(reinterpret_cast<const char *>(&originVal), sizeof(originVal)));
    }

    // Sort output states by stable ID for deterministic fingerprinting
    QList<OutputColorState> sortedOutputs = outputs;
    std::sort(sortedOutputs.begin(), sortedOutputs.end(), [](const OutputColorState &a, const OutputColorState &b) {
        return a.stableId < b.stableId;
    });

    for (const auto &out : sortedOutputs) {
        hash.addData(out.stableId.toUtf8());
        hash.addData(out.activeProfileId.toUtf8());
        const quint32 policyVal = qToBigEndian(static_cast<quint32>(out.appliedAssignment.policy));
        hash.addData(QByteArrayView(reinterpret_cast<const char *>(&policyVal), sizeof(policyVal)));
        const quint32 intentVal = qToBigEndian(static_cast<quint32>(out.appliedAssignment.intent));
        hash.addData(QByteArrayView(reinterpret_cast<const char *>(&intentVal), sizeof(intentVal)));
        const quint32 degradedVal = qToBigEndian(static_cast<quint32>(out.degradedReason));
        hash.addData(QByteArrayView(reinterpret_cast<const char *>(&degradedVal), sizeof(degradedVal)));
        const quint8 isDegraded = out.isDegraded ? 1 : 0;
        hash.addData(QByteArrayView(reinterpret_cast<const char *>(&isDegraded), 1));
    }

    return hash.result();
}

} // namespace QindaQt::DisplayColor
