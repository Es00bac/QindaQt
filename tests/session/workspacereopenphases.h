// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QProcess>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace QindaQt::Test {

class CompositorProbeClient;

// One client-helper process: a real Wayland application identity (desktop
// entry id) owning one or more plainly decorated, painted windows. Window
// titles must stay unique across the whole session because the compositor
// inventory is keyed by title.
struct WorkspaceClientSpec final
{
    QString desktopEntryId;
    QStringList titles;
    QVector<QSize> sizes;
};

// Workspace documents are keyed by durable slot ids; the reopen phase maps
// them to the windows it explicitly chose in the production dialog.
struct WorkspaceSlotChoice final
{
    QString slotId;
    QString windowTitle;
    QString windowId;
};

[[nodiscard]] QVector<QProcess *>
spawnWorkspaceClients(const QVector<WorkspaceClientSpec> &specs,
                      const QStringList &activateTitles, QObject *parent,
                      QString *error);

// Reads the single saved document below $XDG_DATA_HOME/qindaqt/workspaces.
// The probe runs as the compositor session client, so it inherits the exact
// isolated XDG data root the production controller writes to.
[[nodiscard]] std::optional<QJsonObject>
readSavedWorkspaceDocument(QString *error);

[[nodiscard]] std::optional<QJsonObject>
exerciseWorkspaceSavePhase(CompositorProbeClient &client, QString *error);

[[nodiscard]] std::optional<QJsonObject>
exerciseWorkspaceReopenPhase(CompositorProbeClient &client, QString *error);

} // namespace QindaQt::Test
