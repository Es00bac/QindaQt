// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QUrl>
#include <QVector>

namespace QindaQt::Apps::FileManager {

// One saved network location. `url` is always the canonical, userinfo-free
// smb/sftp URL NetworkLocation::canonicalize() produced, so a saved location
// is exactly as browsable as a typed one and can never smuggle a credential
// into the location bar, the breadcrumb, or the state file (ADR-0194).
struct NetworkLocationRecord final {
  // Stable identity derived from the canonical URL, so re-saving the same
  // folder updates the record instead of adding a second card.
  QString id;
  QString name;
  QUrl url;
  // Shown as its own Places sidebar row when true.
  bool showInPlaces = true;
  // ADR-0199: a systemd user `.mount` unit mounts this location under
  // ~/Network at login. Only sftp locations can carry it, and turning it on
  // is what makes `net-fs/sshfs` a runtime dependency -- for this user, on
  // this machine, and nowhere else.
  bool mountAtLogin = false;

  [[nodiscard]] bool operator==(const NetworkLocationRecord &) const = default;
};

enum class NetworkLocationsError {
  None,
  Absent,
  InvalidRoot,
  ReadFailed,
  TooLarge,
  Malformed,
  WriteFailed,
};

struct NetworkLocationsLoadResult final {
  QVector<NetworkLocationRecord> locations;
  NetworkLocationsError error = NetworkLocationsError::None;
  QString diagnostic;
  // True when these records came from a `network-locations-v1` file. The
  // owner persists them once, which writes the v2 document.
  bool migratedFromV1 = false;

  [[nodiscard]] bool ok() const { return error == NetworkLocationsError::None; }
};

struct NetworkLocationsWriteResult final {
  NetworkLocationsError error = NetworkLocationsError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return error == NetworkLocationsError::None; }
};

// AGENT-CONTRACT: owns only the versioned `network-locations-v1` inventory
// beneath the injected state directory, over the shared symlink-refusing
// StateFile primitive (ADR-0090 pattern, same as BookmarksStore). It never
// discovers HOME, contacts a server, resolves a host, or touches a
// credential store.
//
// AGENT-GUARD: every URL that enters or leaves this store must survive
// NetworkLocation::canonicalize() unchanged. A record whose URL carries
// userinfo, an unsupported scheme, an empty host, or a non-canonical path
// makes the whole inventory Malformed rather than being silently dropped --
// a half-loaded inventory would quietly lose a user's saved location.
//
// AGENT-CONTRACT: the reader demands an exact key set, so an inventory
// written by a newer schema is refused rather than partly understood. This is
// `network-locations-v2`; a `network-locations-v1` inventory (no
// `mountAtLogin`) is read once and reported through
// NetworkLocationsLoadResult::migratedFromV1 so composition can rewrite it as
// v2. The v1 file is left where it is rather than deleted, so downgrading to
// an older build loses nothing. Adding a further field (a per-location user
// name, in-place vs copy-on-open) means v3 and the same treatment.
class NetworkLocationsStore final {
public:
  static constexpr qint64 maximumBytes = 64 * 1024;
  static constexpr int schemaVersion = 2;
  static constexpr int maximumLocations = 64;
  static constexpr int maximumNameLength = 256;
  static constexpr int maximumUrlLength = 4096;

  explicit NetworkLocationsStore(QString stateDirectory);

  [[nodiscard]] QString filePath() const;
  // Absent is a clean first-run result: ok() is false but locations is empty
  // and diagnostic is empty, so no error is shown on first launch.
  [[nodiscard]] NetworkLocationsLoadResult load() const;
  [[nodiscard]] NetworkLocationsWriteResult
  store(const QVector<NetworkLocationRecord> &locations) const;

  // The canonical identity of one location URL: the exact canonical URL
  // string. Composition uses it to dedup a re-saved folder.
  [[nodiscard]] static QString identityFor(const QUrl &canonicalUrl);

private:
  // Reads one inventory document of the given schema version. `version` also
  // selects the accepted per-entry key set.
  [[nodiscard]] NetworkLocationsLoadResult loadVersion(int version) const;
  [[nodiscard]] static bool validate(const QVector<NetworkLocationRecord> &locations,
                                     QString *diagnostic);

  QString m_stateDirectory;
};

} // namespace QindaQt::Apps::FileManager
