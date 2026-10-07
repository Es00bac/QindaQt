// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QtCore/QtGlobal>

namespace QindaQt::RemovableMedia {
inline constexpr quint32 kProtocolVersion = 1;
inline constexpr quint32 kCodecVersion = 1;
inline constexpr char kEnvelopeMagic[] = {'Q', 'R', 'M', 'D'};
inline constexpr char kServiceName[] = "org.qindaqt.RemovableMedia1";
inline constexpr char kObjectPath[] = "/org/qindaqt/RemovableMedia1/Devices";
inline constexpr char kInterfaceName[] = "org.qindaqt.RemovableMedia1.Devices";
inline constexpr char kOwnerDesktopId[] = "org.qindaqt.RemovableMedia.desktop";

// AGENT-CONTRACT: ADR-0350 bounds apply before allocation/publication. Never
// truncate rows: hidden sibling loss cannot become removal admission.
inline constexpr qsizetype kMaxDrives = 32;
inline constexpr qsizetype kMaxVolumes = 128;
inline constexpr qsizetype kMaxMountRoots = 8;
inline constexpr qsizetype kMaxSnapshotBytes = 256 * 1024;
inline constexpr qsizetype kMaxActionEnvelopeBytes = 16 * 1024;
inline constexpr qsizetype kMaxMountRootUtf8Bytes = 4096;
inline constexpr qsizetype kMaxLabelUtf8Bytes = 256;
inline constexpr qsizetype kMaxMessageUtf8Bytes = 1024;
inline constexpr qsizetype kMaxIdentifierBytes = 64;
inline constexpr qsizetype kMaxOwnerBytes = 255;
inline constexpr qsizetype kMaxRecentRequests = 64;
inline constexpr qint64 kRecentRequestLifetimeMilliseconds = 5 * 60 * 1000;
} // namespace QindaQt::RemovableMedia
