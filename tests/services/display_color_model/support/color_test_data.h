// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QCryptographicHash>
#include <QtCore/QtEndian>
#include <qindaqt/services/display_color_model/color_limits.h>
#include <qindaqt/services/display_color_model/color_types.h>

namespace QindaQt::DisplayColor::Testing
{

inline QByteArray createValidIccHeader(
    quint32 profileSize = 1024,
    const QByteArray &deviceClass = "mntr",
    const QByteArray &dataColorSpace = "RGB ",
    const QByteArray &connectionSpace = "XYZ ")
{
    QByteArray header(IccHeaderSizeBytes, '\0');
    uchar *bytes = reinterpret_cast<uchar *>(header.data());

    // 0..3: Profile size (big endian)
    const quint32 sizeBe = qToBigEndian(profileSize);
    std::memcpy(bytes, &sizeBe, 4);

    // 4..7: CMM type
    const quint32 cmmBe = qToBigEndian(0x4150504C); // 'APPL'
    std::memcpy(bytes + 4, &cmmBe, 4);

    // 8..11: Version (2.4.0)
    const quint32 verBe = qToBigEndian(0x02400000);
    std::memcpy(bytes + 8, &verBe, 4);

    // 12..15: Device class
    std::memcpy(bytes + 12, deviceClass.constData(), 4);

    // 16..19: Data color space
    std::memcpy(bytes + 16, dataColorSpace.constData(), 4);

    // 20..23: Connection space
    std::memcpy(bytes + 20, connectionSpace.constData(), 4);

    // 36..39: Magic 'acsp'
    const quint32 magicBe = qToBigEndian(IccMagicAcsp);
    std::memcpy(bytes + 36, &magicBe, 4);

    // 84..99: Profile ID (16 bytes MD5)
    for (int i = 0; i < 16; ++i) {
        bytes[84 + i] = static_cast<uchar>(i * 17);
    }

    return header;
}

inline IccProfileDescriptor createSampleProfile(
    const QString &id,
    const QString &displayName,
    ProfileOrigin origin = ProfileOrigin::BuiltIn,
    ColorSpaceGamut gamut = ColorSpaceGamut::Srgb,
    quint32 size = 1024)
{
    IccProfileDescriptor desc;
    desc.profileId = id;
    desc.displayName = displayName;
    desc.description = displayName + " standard profile";
    desc.fileName = id + ".icc";
    desc.origin = origin;
    desc.gamut = gamut;
    desc.transferFunction = TransferFunction::Srgb;
    desc.rawHeader = createValidIccHeader(size);
    desc.byteSize = size;
    desc.checksumSha256 = QCryptographicHash::hash(desc.rawHeader, QCryptographicHash::Sha256);
    desc.wireValid = true;
    return desc;
}

inline OutputColorCapabilities createSampleCapabilities(
    const QString &stableId,
    bool supportsHdr = true,
    bool supportsWcg = true)
{
    OutputColorCapabilities caps;
    caps.stableId = stableId;
    caps.supportsWcg = supportsWcg;
    caps.supportsHdr = supportsHdr;
    caps.supportedGamuts = {ColorSpaceGamut::Srgb};
    if (supportsWcg) {
        caps.supportedGamuts.append(ColorSpaceGamut::DciP3);
        caps.supportedGamuts.append(ColorSpaceGamut::Bt2020);
    }
    caps.supportedTransferFunctions = {TransferFunction::Srgb, TransferFunction::Gamma22};
    if (supportsHdr) {
        caps.supportedTransferFunctions.append(TransferFunction::Pq);
        caps.supportedTransferFunctions.append(TransferFunction::Hlg);
    }
    caps.minLuminanceNits = 0.05;
    caps.maxLuminanceNits = supportsHdr ? 1000.0 : 350.0;
    caps.maxFullFrameLuminanceNits = supportsHdr ? 600.0 : 350.0;
    caps.autoColorManagementAvailable = true;
    caps.wireValid = true;
    return caps;
}

inline OutputColorAssignment createSampleAssignment(
    const QString &stableId,
    const QString &profileId = QString(),
    OutputColorPolicy policy = OutputColorPolicy::SdrSrgb,
    RenderingIntent intent = RenderingIntent::Perceptual)
{
    OutputColorAssignment assignment;
    assignment.stableId = stableId;
    assignment.profileId = profileId;
    assignment.policy = policy;
    assignment.intent = intent;
    assignment.sdrBrightnessNits = DefaultSdrLuminanceNits;
    assignment.wireValid = true;
    return assignment;
}

} // namespace QindaQt::DisplayColor::Testing
