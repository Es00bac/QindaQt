// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "workspace.h"
namespace QindaQt::Workspaces {
// Filesystem adapter. The caller supplies a dedicated root, owns this object,
// and serializes access on its I/O thread. Reads never create directories.
// Save is atomic; failure retains the last document. No launch or adoption.
class WorkspaceStore final {
public:
  explicit WorkspaceStore(QString root) : m_root(std::move(root)) {}
  [[nodiscard]] bool save(const Workspace &, QString *error = nullptr) const;
  [[nodiscard]] std::optional<Workspace> load(const QString &id,
                                              QString *error = nullptr) const;
  [[nodiscard]] QStringList ids() const;

private:
  QString m_root;
};
} // namespace QindaQt::Workspaces
