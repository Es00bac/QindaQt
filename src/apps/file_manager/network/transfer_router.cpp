// SPDX-License-Identifier: GPL-3.0-or-later
#include "transfer_router.h"

#include "network_location.h"

#include <QDir>

namespace QindaQt::Apps::FileManager {
namespace {

[[nodiscard]] TransferRouting refusal(const QString &message) {
  return {.route = TransferRoute::Refuse,
          .sources = {},
          .destinationFolder = {},
          .message = message};
}

[[nodiscard]] bool sameAuthority(const QUrl &left, const QUrl &right) {
  return left.scheme() == right.scheme() && left.host() == right.host() &&
         left.port() == right.port();
}

// AGENT-GUARD: the destination folder may be neither the source itself nor
// anything inside it. This is the one refusal the queue cannot make later --
// KIO would happily start recursing a folder into its own subtree -- so it is
// checked here, before an item is ever created. Returns an empty string when
// the pair is acceptable, and the reason otherwise.
[[nodiscard]] QString selfTargetRefusal(const QUrl &source, const QUrl &destination) {
  if (source.scheme() != destination.scheme() ||
      source.host() != destination.host() || source.port() != destination.port()) {
    return {};
  }
  const QString sourcePath = source.path();
  const QString destinationPath = destination.path();
  if (destinationPath == sourcePath) {
    return QStringLiteral("Choose a destination folder; that address is the item "
                          "being transferred.");
  }
  if (destinationPath.startsWith(sourcePath + QLatin1Char('/'))) {
    return QStringLiteral("A folder cannot be copied or moved into itself.");
  }
  return {};
}

} // namespace

std::optional<QUrl> TransferRouter::normalizeEndpoint(const QString &text) {
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return std::nullopt;
  }
  if (NetworkLocation::classify(trimmed) != LocationScheme::Local) {
    return NetworkLocation::canonicalize(trimmed);
  }
  // AGENT-GUARD: a local endpoint must be an absolute, already-clean path.
  // Cleaning it here instead of refusing would make "/home/x/../etc" a valid
  // transfer target that the user never typed.
  if (!QDir::isAbsolutePath(trimmed) || trimmed.contains(QChar::Null) ||
      !trimmed.isValidUtf16() || QDir::cleanPath(trimmed) != trimmed) {
    return std::nullopt;
  }
  return QUrl::fromLocalFile(trimmed);
}

TransferRealm TransferRouter::realmOf(const QUrl &endpoint) {
  return NetworkLocation::isSupportedScheme(endpoint) ? TransferRealm::Remote
                                                      : TransferRealm::Local;
}

TransferRouting TransferRouter::route(const QStringList &sourcePaths,
                                      const QString &destinationText) {
  if (sourcePaths.isEmpty()) {
    return refusal(QStringLiteral("Select at least one item to transfer."));
  }
  if (sourcePaths.size() > maximumSources) {
    return refusal(QStringLiteral("Transfer at most %1 items at a time.")
                       .arg(maximumSources));
  }
  const auto destination = normalizeEndpoint(destinationText);
  if (!destination.has_value()) {
    return refusal(QStringLiteral(
        "Enter an absolute folder path, or an sftp:// or smb:// address."));
  }

  QVector<QUrl> sources;
  sources.reserve(sourcePaths.size());
  bool anyRemote = realmOf(*destination) == TransferRealm::Remote;
  bool allSameAuthorityAsDestination = true;
  for (const QString &sourcePath : sourcePaths) {
    const auto source = normalizeEndpoint(sourcePath);
    if (!source.has_value()) {
      return refusal(QStringLiteral("One of the selected items has an address "
                                    "that cannot be transferred."));
    }
    const QString selfTarget = selfTargetRefusal(*source, *destination);
    if (!selfTarget.isEmpty()) {
      return refusal(selfTarget);
    }
    if (realmOf(*source) == TransferRealm::Remote) {
      anyRemote = true;
    }
    if (!sameAuthority(*source, *destination)) {
      allSameAuthorityAsDestination = false;
    }
    sources.append(*source);
  }

  TransferRouting routing;
  routing.sources = std::move(sources);
  routing.destinationFolder = *destination;
  if (!anyRemote) {
    routing.route = TransferRoute::LocalMutation;
    return routing;
  }
  // The accepted single-child remote path stays the owner of exactly the
  // case it was reviewed for; everything wider is the queue's.
  routing.route = (routing.sources.size() == 1 &&
                   realmOf(routing.sources.constFirst()) == TransferRealm::Remote &&
                   realmOf(*destination) == TransferRealm::Remote &&
                   allSameAuthorityAsDestination)
                      ? TransferRoute::RemoteSameAuthorityChild
                      : TransferRoute::Queue;
  return routing;
}

} // namespace QindaQt::Apps::FileManager
