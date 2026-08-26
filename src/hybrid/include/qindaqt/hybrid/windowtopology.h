// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "validationresult.h"
#include "windowcontainer.h"

#include <QMap>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <QtTypes>

#include <optional>

namespace QindaQt::Hybrid {

class TopologyMutation;
class TopologyMutationAccess;

// A topology owns only domain values, never compositor objects. Copies are
// cheap enough for command staging because Qt containers are implicitly shared;
// WindowContainer trees detach only where a mutation occurs.
class WindowTopology final
{
public:
    WindowTopology() = default;

    [[nodiscard]] static std::optional<WindowTopology> create(
        QStringList independentWindowIds,
        QVector<Core::WindowContainer> containers = {},
        quint64 revision = 0,
        QString *error = nullptr);

    [[nodiscard]] quint64 revision() const noexcept { return m_revision; }
    [[nodiscard]] QStringList containerIds() const { return m_containers.keys(); }
    [[nodiscard]] QStringList independentWindowIds() const;
    [[nodiscard]] const Core::WindowContainer *container(const QString &containerId) const noexcept;
    [[nodiscard]] bool isIndependent(const QString &windowId) const noexcept;
    [[nodiscard]] std::optional<QString> ownerOf(const QString &windowId) const;
    [[nodiscard]] QStringList windowIds(const QString &containerId) const;
    [[nodiscard]] Core::ValidationResult validate() const;

private:
    void swap(WindowTopology &other) noexcept;

    quint64 m_revision = 0;
    QMap<QString, Core::WindowContainer> m_containers;
    QSet<QString> m_independentWindows;

    friend class TopologyMutation;
    friend class TopologyMutationAccess;
    friend class TopologyCoordinator;
    friend class TopologyRepository;
};

// Repository references remain valid only until the next successful publish.
// The repository has no thread synchronization; its coordinator and all scene
// transaction callbacks must run serially on the owning compositor thread.
class TopologyRepository final
{
public:
    explicit TopologyRepository(WindowTopology initial = {});

    [[nodiscard]] const WindowTopology &topology() const noexcept { return m_topology; }

private:
    void publish(WindowTopology &&candidate) noexcept;

    WindowTopology m_topology;

    friend class TopologyCoordinator;
};

} // namespace QindaQt::Hybrid
