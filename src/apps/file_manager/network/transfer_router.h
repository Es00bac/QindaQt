// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "transfer_types.h"

#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>

#include <optional>

namespace QindaQt::Apps::FileManager {

// Which collaborator owns one Copy To / Move To request.
enum class TransferRoute {
  // Every endpoint is local: MutationController keeps the request, with its
  // identity checks, cross-device refusal, Trash, and undo.
  LocalMutation,
  // Exactly one source, both endpoints on one network authority: the
  // accepted ADR-0155/0156 single-child path keeps the request. That path's
  // narrow contract is deliberate and this router never widens it.
  RemoteSameAuthorityChild,
  // Anything else that involves a network endpoint: local to network,
  // network to local, network to a different authority, or a multi-item
  // network selection. TransferQueueController owns these.
  Queue,
  // The request cannot be expressed at all (an unusable source or
  // destination, or a destination inside its own source).
  Refuse,
};

struct TransferRouting final {
  TransferRoute route = TransferRoute::Refuse;
  // Normalized endpoints, filled for every route but Refuse. Local
  // endpoints are file:// URLs; network endpoints are canonical smb/sftp.
  QVector<QUrl> sources;
  QUrl destinationFolder;
  // Bounded human-readable reason, set only for Refuse.
  QString message;
};

// AGENT-CONTRACT: the one place a Copy To / Move To request is assigned an
// owner. Pure policy: it performs no I/O, stats nothing, resolves no symlink,
// and contacts no server, so every decision is deterministic and testable.
// It decides *who* runs a transfer, never whether the source exists -- that
// answer belongs to the collaborator that actually runs it.
class TransferRouter final {
public:
  // A queued batch is bounded so a stray select-all over a huge folder
  // cannot make an unbounded number of network jobs.
  static constexpr int maximumSources = 512;

  // Turns one entry path or typed destination into a URL: an absolute local
  // path becomes file://, a canonical smb/sftp string stays one. Returns
  // nullopt for a relative path, a non-canonical address, an unsupported
  // scheme, or anything carrying userinfo.
  [[nodiscard]] static std::optional<QUrl> normalizeEndpoint(const QString &text);
  [[nodiscard]] static TransferRealm realmOf(const QUrl &endpoint);

  // sourcePaths are entry paths exactly as the listing published them;
  // destinationText is what the destination dialog holds.
  [[nodiscard]] static TransferRouting route(const QStringList &sourcePaths,
                                             const QString &destinationText);
};

} // namespace QindaQt::Apps::FileManager
