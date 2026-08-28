// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "file_manager_types.h"

#include <memory>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: An implementation performs bounded synchronous local I/O
// only and is safe to call from the GUI thread for one directory at a time.
// It never displays UI, mutates navigation state, or resolves a network,
// mount, or portal location; those remain later slices. A failure returns a
// typed ListingError plus a diagnostic string instead of throwing or blocking
// indefinitely.
class DirectoryLister {
public:
  virtual ~DirectoryLister() = default;

  [[nodiscard]] virtual ListingResult list(const QString &absolutePath) const = 0;
};

using DirectoryListerPtr = std::unique_ptr<DirectoryLister>;

} // namespace QindaQt::Apps::FileManager
