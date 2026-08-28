// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>

#include "color_limits.h"

namespace QindaQt::DisplayColor
{

enum class ProfileOrigin : quint32 {
    BuiltIn = 0,
    System = 1,
    UserImported = 2,
    EdidDerived = 3,
};

enum class RenderingIntent : quint32 {
    Perceptual = 0,
    RelativeColorimetric = 1,
    Saturation = 2,
    AbsoluteColorimetric = 3,
};

enum class ColorSpaceGamut : quint32 {
    Srgb = 0,
    DciP3 = 1,
    Bt2020 = 2,
    AdobeRgb = 3,
    Custom = 4,
};

enum class TransferFunction : quint32 {
    Srgb = 0,
    Linear = 1,
    Pq = 2,       // SMPTE ST 2084 / HDR10
    Hlg = 3,      // BT.2100 HLG
    Gamma22 = 4,
};

enum class OutputColorPolicy : quint32 {
    SdrSrgb = 0,
    SdrWcg = 1,
    HdrEnabled = 2,
    AutoColorManagement = 3,
};

enum class DegradedReason : quint32 {
    None = 0,
    ProfileNotFound = 1,
    ProfileInvalid = 2,
    HdrUnsupported = 3,
    WcgUnsupported = 4,
    AmbiguousIdentity = 5,
    StaleLineage = 6,
    HardwareDegraded = 7,
};

enum class ProfileValidationStatus : quint32 {
    Valid = 0,
    EmptyData = 1,
    HeaderTooSmall = 2,
    InvalidMagic = 3,
    InvalidDeclaredSize = 4,
    InvalidSize = 5,
    UnsupportedColorSpace = 6,
    UnsupportedConnectionSpace = 7,
    UnsupportedProfileClass = 8,
    ChecksumMismatch = 9,
    Oversized = 10,
    MalformedMetadata = 11,
    InvalidVersion = 12,
};

struct IccHeaderSummary {
    quint32 profileSize = 0;
    quint32 cmmType = 0;
    quint32 version = 0;
    QByteArray deviceClass;
    QByteArray dataColorSpace;
    QByteArray connectionSpace;
    QByteArray profileIdMd5;
    bool valid = false;

    friend bool operator==(const IccHeaderSummary &, const IccHeaderSummary &) = default;
};

struct IccProfileDescriptor {
    QString profileId;
    QString displayName;
    QString description;
    QString fileName;
    ProfileOrigin origin = ProfileOrigin::BuiltIn;
    ColorSpaceGamut gamut = ColorSpaceGamut::Srgb;
    TransferFunction transferFunction = TransferFunction::Srgb;
    QByteArray rawHeader;
    QByteArray checksumSha256;
    quint32 byteSize = 0;
    bool wireValid = true;

    friend bool operator==(const IccProfileDescriptor &, const IccProfileDescriptor &) = default;
};

struct OutputColorCapabilities {
    QString stableId;
    bool supportsWcg = false;
    bool supportsHdr = false;
    QList<ColorSpaceGamut> supportedGamuts;
    QList<TransferFunction> supportedTransferFunctions;
    double minLuminanceNits = 0.0;
    double maxLuminanceNits = 300.0;
    double maxFullFrameLuminanceNits = 300.0;
    bool autoColorManagementAvailable = false;
    bool wireValid = true;

    friend bool operator==(const OutputColorCapabilities &, const OutputColorCapabilities &) = default;
};

struct OutputColorAssignment {
    QString stableId;
    QString profileId;
    OutputColorPolicy policy = OutputColorPolicy::SdrSrgb;
    RenderingIntent intent = RenderingIntent::Perceptual;
    bool sdrBrightnessGainApplied = false;
    double sdrBrightnessNits = DefaultSdrLuminanceNits;
    bool wireValid = true;

    friend bool operator==(const OutputColorAssignment &, const OutputColorAssignment &) = default;
};

struct OutputColorState {
    QString stableId;
    OutputColorCapabilities capabilities;
    OutputColorAssignment appliedAssignment;
    OutputColorAssignment requestedAssignment;
    DegradedReason degradedReason = DegradedReason::None;
    bool isDegraded = false;
    QString activeProfileId;
    bool wireValid = true;

    friend bool operator==(const OutputColorState &, const OutputColorState &) = default;
};

struct ColorCatalog {
    QList<IccProfileDescriptor> profiles;
    QString defaultSrgbProfileId;
    bool wireValid = true;

    friend bool operator==(const ColorCatalog &, const ColorCatalog &) = default;
};

struct ColorModelSnapshot {
    quint32 schemaVersion = 1;
    QString serviceEpoch;
    quint64 revision = 0;
    QByteArray lineageFingerprint;
    ColorCatalog catalog;
    QList<OutputColorState> outputs;
    bool wireValid = true;

    friend bool operator==(const ColorModelSnapshot &, const ColorModelSnapshot &) = default;
};

} // namespace QindaQt::DisplayColor

Q_DECLARE_METATYPE(QindaQt::DisplayColor::ProfileOrigin)
Q_DECLARE_METATYPE(QindaQt::DisplayColor::RenderingIntent)
Q_DECLARE_METATYPE(QindaQt::DisplayColor::ColorSpaceGamut)
Q_DECLARE_METATYPE(QindaQt::DisplayColor::TransferFunction)
Q_DECLARE_METATYPE(QindaQt::DisplayColor::OutputColorPolicy)
Q_DECLARE_METATYPE(QindaQt::DisplayColor::DegradedReason)
Q_DECLARE_METATYPE(QindaQt::DisplayColor::ProfileValidationStatus)
Q_DECLARE_METATYPE(QindaQt::DisplayColor::IccHeaderSummary)
Q_DECLARE_METATYPE(QindaQt::DisplayColor::IccProfileDescriptor)
Q_DECLARE_METATYPE(QindaQt::DisplayColor::OutputColorCapabilities)
Q_DECLARE_METATYPE(QindaQt::DisplayColor::OutputColorAssignment)
Q_DECLARE_METATYPE(QindaQt::DisplayColor::OutputColorState)
Q_DECLARE_METATYPE(QindaQt::DisplayColor::ColorCatalog)
Q_DECLARE_METATYPE(QindaQt::DisplayColor::ColorModelSnapshot)
