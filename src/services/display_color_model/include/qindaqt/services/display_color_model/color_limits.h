// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtTypes>

namespace QindaQt::DisplayColor
{

// AGENT-CONTRACT: Display Color C0 enforces strict upper bounds on sizes,
// counts, and string lengths to prevent unbounded memory allocation and
// hostile IPC/file input injection across process boundaries.

inline constexpr quint32 MaxIccProfileSizeBytes = 4 * 1024 * 1024; // 4 MiB
inline constexpr quint32 MinIccProfileSizeBytes = 128;              // Standard ICC header size
inline constexpr quint32 IccHeaderSizeBytes = 128;
inline constexpr quint32 MaxProfilesInCatalog = 256;
inline constexpr quint32 MaxOutputs = 32;

inline constexpr int MaxIdentifierLength = 128;
inline constexpr int MaxDisplayNameLength = 128;
inline constexpr int MaxDescriptionLength = 512;
inline constexpr int MaxFilenameLength = 255;

inline constexpr double MinLuminanceNits = 0.0;
inline constexpr double MaxLuminanceNits = 10000.0;
inline constexpr double DefaultSdrLuminanceNits = 200.0;

// ICC specification constants
inline constexpr quint32 IccMagicAcsp = 0x61637370; // 'acsp' in big-endian

} // namespace QindaQt::DisplayColor
