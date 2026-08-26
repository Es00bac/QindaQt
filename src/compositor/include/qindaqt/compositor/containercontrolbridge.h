// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "controltypes.h"

#include "windowcontainer.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>

#include <optional>

namespace QindaQt::Compositor {

class SceneAdapter;

class ContainerControlBridge final : public QObject
{
    Q_OBJECT

public:
    // adapter is non-owning and must outlive the bridge. Calls and destruction
    // are confined to the bridge's QObject thread (KWin's compositor thread).
    explicit ContainerControlBridge(SceneAdapter &adapter, QObject *parent = nullptr);

    [[nodiscard]] bool registerContainer(Core::WindowContainer container,
                                         QString *error = nullptr);
    [[nodiscard]] bool unregisterContainer(const QString &containerId,
                                           QString *error = nullptr);
    [[nodiscard]] bool contains(const QString &containerId) const;
    [[nodiscard]] std::optional<quint64> revision(const QString &containerId) const;
    [[nodiscard]] std::optional<QJsonObject> snapshot(const QString &containerId) const;

    // Atomically promotes a private one-leaf staging model with exactly one
    // split-window operation. Rejections remove staging before returning, so
    // callers cannot observe a registered singleton between API calls.
    [[nodiscard]] ControlReply submitStagedSplit(Core::WindowContainer staging,
                                                 const ControlRequest &request);
    [[nodiscard]] ControlReply submit(const ControlRequest &request);

Q_SIGNALS:
    void containerCommitted(const QString &containerId,
                            quint64 revision,
                            const QJsonObject &snapshot);

private:
    struct ContainerState final
    {
        Core::WindowContainer container;
        quint64 revision = 0;
    };

    [[nodiscard]] ControlReply reject(const ControlRequest &request,
                                      ReplyStatus status,
                                      QString code,
                                      QString message,
                                      qsizetype operationIndex = -1,
                                      quint64 revision = 0) const;
    void assertThread() const;

    SceneAdapter &m_adapter;
    QHash<QString, ContainerState> m_containers;
    bool m_applying = false;
};

} // namespace QindaQt::Compositor
