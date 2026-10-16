// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QSize>
#include <limits>
#include <optional>
namespace QindaQt::LockProtocol {
// AGENT-CONTRACT: ext_session_lock_surface_v1.configure asks the client to
// resize; it is never a reason to exit. A compositor can legitimately announce a
// zero dimension while an output is being reconfigured. Such a configure is
// SKIPPED: neither applied nor acknowledged, so the surface keeps its last
// acknowledged state, the compositor keeps the output covered by its own
// protected fallback, and the next usable configure is handled normally. The
// same holds for sizes a QSize cannot hold. Aborting here (the previous
// qFatal) left the whole session on a black screen (ADR-0382).
inline std::optional<QSize> usableConfigureSize(unsigned width, unsigned height) {
  constexpr unsigned limit = static_cast<unsigned>(std::numeric_limits<int>::max());
  if (width == 0 || height == 0 || width > limit || height > limit) return std::nullopt;
  return QSize(static_cast<int>(width), static_cast<int>(height));
}
} // namespace QindaQt::LockProtocol
