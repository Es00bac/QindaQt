// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/hybrid/topologycommand.h>
#include <qindaqt/hybrid/windowtopology.h>

namespace QindaQt::Hybrid {

class TopologyMutationAccess final
{
public:
    [[nodiscard]] static QMap<QString, Core::WindowContainer> &containers(
        WindowTopology &topology)
    {
        return topology.m_containers;
    }

    [[nodiscard]] static QSet<QString> &independentWindows(WindowTopology &topology)
    {
        return topology.m_independentWindows;
    }
};

class TopologyPlacementMutation final
{
public:
    [[nodiscard]] static bool apply(WindowTopology &candidate,
                                    const InsertIndependentWindow &command,
                                    QString *error);
    [[nodiscard]] static bool apply(WindowTopology &candidate,
                                    const GroupIndependentWindowsAsPages &command,
                                    QString *error);
    [[nodiscard]] static bool apply(WindowTopology &candidate,
                                    const RegroupMemberWithIndependent &command,
                                    QString *error);
    [[nodiscard]] static bool apply(WindowTopology &candidate,
                                    const MoveMemberToPage &command,
                                    QString *error);
    [[nodiscard]] static bool apply(WindowTopology &candidate,
                                    const RegroupPageWithIndependent &command,
                                    QString *error);
    [[nodiscard]] static bool apply(WindowTopology &candidate,
                                    const ReparentMember &command,
                                    QString *error);
};

// Private candidate editor. The coordinator is the only supported public
// mutation surface; keeping this type private prevents callers from bypassing
// scene preparation or revision publication.
class TopologyMutation final
{
public:
    [[nodiscard]] static bool apply(WindowTopology &candidate,
                                    const TopologyCommand &command,
                                    QString *error);
    static void normalize(WindowTopology &candidate);
};

} // namespace QindaQt::Hybrid
