// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "network_locations_store.h"

#include <QString>

namespace QindaQt::Apps::FileManager {

enum class MountUnitError {
  None,
  // Only sftp can be mounted: sshfs is the one FUSE filesystem this knob
  // knows, and there is no supported SMB equivalent that needs no root.
  UnsupportedScheme,
  // The location's name cannot become a directory name.
  UnusableName,
  UnusableHome,
};

struct MountUnitResult final {
  // The escaped systemd unit file name, e.g.
  // "home-cabewse-Network-Storage\x20\x28desktop\x29.mount".
  QString unitName;
  // The full unit file text.
  QString contents;
  // The absolute mount point the unit uses.
  QString mountPoint;
  MountUnitError error = MountUnitError::None;
  QString message;

  [[nodiscard]] bool ok() const { return error == MountUnitError::None; }
};

// AGENT-CONTRACT: turns one saved network location into the systemd user
// `.mount` unit that mounts it under ~/Network (ADR-0199). Pure policy: it
// performs no I/O, creates no directory, starts nothing, and knows nothing
// about systemd's running state. The writer and the unit control are separate
// collaborators precisely so this text is testable without a service manager.
//
// AGENT-GUARD: the unit file name is systemd's path escaping of the mount
// point and nothing else. systemd resolves a `.mount` unit's Where= from its
// own name, so a name that is not the escaped path yields a unit that refuses
// to load -- or, worse, one that mounts somewhere the user did not choose.
class MountUnit final {
public:
  // The fixed parent of every QindaQt network mount. Keeping it fixed is what
  // lets the user find, inspect and unmount them without QindaQt.
  static constexpr auto parentDirectoryName = "Network";

  // homeDirectory is injected so a test never touches the real home.
  [[nodiscard]] static MountUnitResult build(const NetworkLocationRecord &location,
                                             const QString &homeDirectory);

  // systemd's path escaping (`systemd-escape --path`), reimplemented so the
  // unit name can be computed without running a helper binary.
  [[nodiscard]] static QString escapePath(const QString &absolutePath);
  // The location name reduced to one safe directory component, or an empty
  // string when nothing usable is left.
  [[nodiscard]] static QString directoryNameFor(const QString &locationName);
};

} // namespace QindaQt::Apps::FileManager
