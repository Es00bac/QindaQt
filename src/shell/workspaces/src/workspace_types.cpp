// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/workspaces/workspace_types.h"

#include <QSet>

#include <algorithm>

namespace QindaQt::Shell::Workspaces {

namespace {

WorkspaceValidation rejected(const char *reason)
{
  WorkspaceValidation validation;
  validation.reasonCode = QString::fromLatin1(reason);
  return validation;
}

} // namespace

WorkspaceValidation validateWorkspaceSnapshot(const WorkspaceSnapshot &snapshot)
{
  if (snapshot.desktops.isEmpty()) {
    return rejected("no-desktops");
  }
  if (snapshot.desktops.size() > Bounds::maxWorkspaces) {
    return rejected("too-many-desktops");
  }
  if (snapshot.rows == 0) {
    return rejected("zero-rows");
  }
  QSet<QString> ids;
  WorkspaceValidation validation;
  validation.snapshot = snapshot;
  for (WorkspaceRow &row : validation.snapshot.desktops) {
    if (row.id.isEmpty() || row.id.size() > Bounds::maxIdLength) {
      return rejected("invalid-desktop-id");
    }
    if (ids.contains(row.id)) {
      return rejected("duplicate-desktop-id");
    }
    ids.insert(row.id);
    if (row.position < 0) {
      return rejected("negative-position");
    }
    row.name = row.name.left(Bounds::maxNameLength);
  }
  if (!ids.contains(snapshot.currentId)) {
    return rejected("current-desktop-unknown");
  }
  std::sort(validation.snapshot.desktops.begin(),
            validation.snapshot.desktops.end(),
            [](const WorkspaceRow &left, const WorkspaceRow &right) {
              if (left.position != right.position) {
                return left.position < right.position;
              }
              return left.id < right.id;
            });
  validation.ok = true;
  return validation;
}

QString workspacePhaseText(WorkspacePhase phase)
{
  switch (phase) {
  case WorkspacePhase::Loading:
    return QStringLiteral("loading");
  case WorkspacePhase::Ready:
    return QStringLiteral("ready");
  case WorkspacePhase::Degraded:
    return QStringLiteral("degraded");
  case WorkspacePhase::Unavailable:
    break;
  }
  return QStringLiteral("unavailable");
}

} // namespace QindaQt::Shell::Workspaces
