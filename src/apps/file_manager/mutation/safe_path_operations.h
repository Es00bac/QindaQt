// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mutation_types.h"

namespace QindaQt::Apps::FileManager {

struct SafeFileReadResult {
  MutationResult result;
  QByteArray contents;
};

// AGENT-CONTRACT: Every parent component is opened with O_NOFOLLOW and remains
// pinned while the leaf mutation executes. Policy validation still belongs to
// LocalMutationBackend and HomeTrash.
[[nodiscard]] MutationResult ensureLocalDirectoryNoFollow(
    const QString &path, quint32 permissions);

[[nodiscard]] MutationResult writeExclusiveLocalFileNoFollow(
    const QString &path, const QByteArray &contents, quint32 permissions);

[[nodiscard]] SafeFileReadResult readLocalFileNoFollow(
    const QString &path, qsizetype maximumBytes);

[[nodiscard]] MutationResult createLocalDirectoryNoFollow(
    const QString &destination, const FileIdentity &expectedParent);

[[nodiscard]] MutationResult relocateLocalNoFollow(
    const QString &source, const QString &destination,
    const FileIdentity &expectedSource,
    const FileIdentity &expectedDestinationParent);

} // namespace QindaQt::Apps::FileManager
