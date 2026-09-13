// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../network/network_directory_backend.h"
#include "directory_lister.h"

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

// Local listing error -> status mapping (used by the local reload path).
[[nodiscard]] inline NavigationStatus statusFor(const ListingResult &result) {
  if (result.ok()) {
    return result.entries.isEmpty() ? NavigationStatus::Empty : NavigationStatus::Ready;
  }
  switch (result.error) {
  case ListingError::NotFound:
    return NavigationStatus::Missing;
  case ListingError::PermissionDenied:
    return NavigationStatus::PermissionDenied;
  case ListingError::NotADirectory:
    return NavigationStatus::NotADirectory;
  case ListingError::Unknown:
  case ListingError::None:
    break;
  }
  return NavigationStatus::Error;
}

// Typed remote listing error -> status mapping.
[[nodiscard]] inline NavigationStatus statusForNetworkError(NetworkListingError error) {
  switch (error) {
  case NetworkListingError::None:
    return NavigationStatus::Ready;
  case NetworkListingError::Unavailable:
    return NavigationStatus::Unavailable;
  case NetworkListingError::AuthenticationRequired:
    return NavigationStatus::AuthenticationRequired;
  case NetworkListingError::PermissionDenied:
    return NavigationStatus::PermissionDenied;
  case NetworkListingError::NotFound:
    return NavigationStatus::Missing;
  case NetworkListingError::Transport:
    return NavigationStatus::Transport;
  case NetworkListingError::Unknown:
    break;
  }
  return NavigationStatus::Error;
}

} // namespace NavigationPresentation

} // namespace QindaQt::Apps::FileManager
