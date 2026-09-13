// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::Apps::FileManager {

// Listing/navigation states published to QML as statusKey() strings. Kept
// outside NavigationController (with its key mapping below) so the
// controller stays under the project's source-size invariant; the values
// are a presentation contract, not controller state.
enum class NavigationStatus {
  Ready,
  Empty,
  PermissionDenied,
  Missing,
  NotADirectory,
  Error,
  // S5 network-browsing states (see NetworkListingError): an in-flight
  // asynchronous remote listing, and the remote-only typed failures a local
  // listing can never produce.
  Loading,
  Unavailable,
  AuthenticationRequired,
  Transport,
};

namespace NavigationPresentation {

[[nodiscard]] inline QString statusKeyFor(NavigationStatus status) {
  switch (status) {
  case NavigationStatus::Ready:
    return QStringLiteral("ready");
  case NavigationStatus::Empty:
    return QStringLiteral("empty");
  case NavigationStatus::PermissionDenied:
    return QStringLiteral("permission-denied");
  case NavigationStatus::Missing:
    return QStringLiteral("missing");
  case NavigationStatus::NotADirectory:
    return QStringLiteral("not-a-directory");
  case NavigationStatus::Error:
    return QStringLiteral("error");
  case NavigationStatus::Loading:
    return QStringLiteral("loading");
  case NavigationStatus::Unavailable:
    return QStringLiteral("unavailable");
  case NavigationStatus::AuthenticationRequired:
    return QStringLiteral("authentication-required");
  case NavigationStatus::Transport:
    return QStringLiteral("transport-error");
  }
  return QStringLiteral("error");
}

} // namespace NavigationPresentation

} // namespace QindaQt::Apps::FileManager
