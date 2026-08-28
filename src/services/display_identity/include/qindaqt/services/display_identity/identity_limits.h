// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

namespace QindaQt::DisplayIdentity
{

inline constexpr quint32 kRegistrySchemaVersion = 2;
inline constexpr qsizetype kMaxConnectedOutputs = 32;
inline constexpr qsizetype kMaxRegistryEntries = 64;
inline constexpr qsizetype kMaxAliases = 32;
inline constexpr qsizetype kMaxConnectorUtf8Bytes = 128;
inline constexpr qsizetype kMaxCompositorUuidUtf8Bytes = 128;
inline constexpr qsizetype kMaxEdidIdentifierBytes = 1'024;
inline constexpr qsizetype kMaxRawEdidBytes = 4'096;
inline constexpr qsizetype kMaxMstPathUtf8Bytes = 256;
inline constexpr qsizetype kMaxStableIdUtf8Bytes = 128;
inline constexpr qsizetype kMaxAliasUtf8Bytes = 64;
inline constexpr qsizetype kMaxLabelUtf8Bytes = 256;
inline constexpr qsizetype kMaxManufacturerUtf8Bytes = 128;
inline constexpr qsizetype kMaxModelUtf8Bytes = 256;
inline constexpr qsizetype kMaxRegistryJsonBytes = 262'144;
inline constexpr qsizetype kDigestBytes = 16;

} // namespace QindaQt::DisplayIdentity
