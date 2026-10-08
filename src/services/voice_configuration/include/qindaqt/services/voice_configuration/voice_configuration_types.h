// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QString>
#include <QtGlobal>
namespace QindaQt::Services::VoiceConfiguration {
inline constexpr auto Interface = "org.qindaqt.VoiceConfiguration1";
inline constexpr auto ObjectPath = "/org/qindaqt/VoiceConfiguration1";
inline constexpr qsizetype MaximumKeyBytes = 512;
inline constexpr qsizetype MaximumIdentifierBytes = 64;
enum class Operation : quint32 { SaveElevenLabsKey = 0, ReloadCredentials = 1 };
enum class Status : quint32 { Succeeded = 0, Rejected = 1, Failed = 2,
                             Uncertain = 3, Busy = 4, Conflict = 5 };
// Value copies contain no credential material. Independent configuration
// revision, not Voice1's transcript revision; schema major remains separate.
struct Snapshot final {
  quint64 revision = 0;
  QString configuredProvider;
  QString effectiveProvider;
  QString credentialSource;
  QString statusCode;
  bool fallbackActive = false;
  bool credentialCached = false;
  bool environmentOverride = false;
  bool canConfigure = false;
  bool operator==(const Snapshot &) const = default;
};
struct Result final {
  quint64 requestId = 0;
  quint64 revision = 0;
  Operation operation = Operation::ReloadCredentials;
  Status status = Status::Rejected;
  QString reasonCode;
};
} // namespace QindaQt::Services::VoiceConfiguration
