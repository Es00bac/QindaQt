// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mutation_types.h"

#include <memory>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: execute() is synchronous and called only from the mutation
// controller's worker task. Implementations own no UI/QObject and report all
// expected failures as values. The cancellation flag and progress callback
// remain valid for the complete call; implementations must poll at bounded
// item boundaries and never invoke the callback after returning.
class MutationBackend {
public:
  virtual ~MutationBackend() = default;

  [[nodiscard]] virtual MutationResult
  execute(const MutationRequest &request,
          const MutationCancellation &cancellation,
          const MutationProgressCallback &progress) = 0;
};

using MutationBackendPtr = std::unique_ptr<MutationBackend>;

} // namespace QindaQt::Apps::FileManager
