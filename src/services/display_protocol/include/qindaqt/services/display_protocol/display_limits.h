// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

namespace QindaQt::Display
{

inline constexpr quint32 kProtocolVersion = 1;
inline constexpr quint32 kCanonicalCodecVersion = 1;

inline constexpr qsizetype kMaxOutputs = 32;
inline constexpr qsizetype kMaxModesPerOutput = 128;
inline constexpr qsizetype kMaxTransactions = 1;
inline constexpr qsizetype kMaxCandidateOutputs = kMaxOutputs;
inline constexpr qsizetype kMaxSerializedBytes = 1'048'576;
inline constexpr quint32 kMaximumRevertAttempts = 3;

inline constexpr qsizetype kMaxStableIdUtf8Bytes = 128;
inline constexpr qsizetype kMaxConnectorNameUtf8Bytes = 128;
inline constexpr qsizetype kMaxRuntimeUuidUtf8Bytes = 128;
inline constexpr qsizetype kMaxLabelUtf8Bytes = 256;
inline constexpr qsizetype kMaxManufacturerUtf8Bytes = 128;
inline constexpr qsizetype kMaxModelUtf8Bytes = 256;
inline constexpr qsizetype kMaxModeIdUtf8Bytes = 128;
inline constexpr qsizetype kMaxServiceEpochUtf8Bytes = 128;
inline constexpr qsizetype kMaxTransactionIdUtf8Bytes = 128;
inline constexpr qsizetype kMaxDiagnosticUtf8Bytes = 512;

inline constexpr int kMaxPixelDimension = 16'384;
inline constexpr int kMaxPhysicalDimensionMillimeters = 10'000;
inline constexpr qint32 kCoordinateBound = 1'000'000;
inline constexpr quint32 kMaxRefreshMilliHertz = 1'000'000;
inline constexpr double kMinimumScale = 1.0;
inline constexpr double kMaximumScale = 3.0;
inline constexpr qsizetype kFingerprintBytes = 32;

inline constexpr char kServiceName[] = "org.qindaqt.Display1";
inline constexpr char kObjectPath[] = "/org/qindaqt/Display1";
inline constexpr char kInterfaceName[] = "org.qindaqt.Display1";

} // namespace QindaQt::Display
