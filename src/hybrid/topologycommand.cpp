// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/hybrid/topologycommand.h>

#include <type_traits>

namespace QindaQt::Hybrid {

TopologyCommandKind commandKind(const TopologyCommand &command) noexcept
{
    return std::visit(
        []<typename Command>(const Command &) {
            if constexpr (std::is_same_v<Command, AddIndependentWindow>) {
                return TopologyCommandKind::AddIndependentWindow;
            } else if constexpr (std::is_same_v<Command, ForgetWindow>) {
                return TopologyCommandKind::ForgetWindow;
            } else if constexpr (std::is_same_v<Command, DockIndependentWindows>) {
                return TopologyCommandKind::DockIndependentWindows;
            } else if constexpr (std::is_same_v<Command, MergeContainers>) {
                return TopologyCommandKind::MergeContainers;
            } else if constexpr (std::is_same_v<Command, MovePage>) {
                return TopologyCommandKind::MovePage;
            } else if constexpr (std::is_same_v<Command, DetachPage>) {
                return TopologyCommandKind::DetachPage;
            } else if constexpr (std::is_same_v<Command, MoveMemberToPage>) {
                return TopologyCommandKind::MoveMemberToPage;
            } else if constexpr (
                std::is_same_v<Command, RegroupPageWithIndependent>) {
                return TopologyCommandKind::RegroupPageWithIndependent;
            } else if constexpr (std::is_same_v<Command, InsertIndependentWindow>) {
                return TopologyCommandKind::InsertIndependentWindow;
            } else if constexpr (
                std::is_same_v<Command, GroupIndependentWindowsAsPages>) {
                return TopologyCommandKind::GroupIndependentWindowsAsPages;
            } else if constexpr (
                std::is_same_v<Command, RegroupMemberWithIndependent>) {
                return TopologyCommandKind::RegroupMemberWithIndependent;
            } else if constexpr (std::is_same_v<Command, MoveMember>) {
                return TopologyCommandKind::MoveMember;
            } else if constexpr (std::is_same_v<Command, ReorderPage>) {
                return TopologyCommandKind::ReorderPage;
            } else if constexpr (std::is_same_v<Command, ActivatePage>) {
                return TopologyCommandKind::ActivatePage;
            } else if constexpr (std::is_same_v<Command, ResizeSplit>) {
                return TopologyCommandKind::ResizeSplit;
            } else if constexpr (std::is_same_v<Command, ReorderMembers>) {
                return TopologyCommandKind::ReorderMembers;
            } else if constexpr (std::is_same_v<Command, ReparentMember>) {
                return TopologyCommandKind::ReparentMember;
            } else if constexpr (std::is_same_v<Command, DetachMember>) {
                return TopologyCommandKind::DetachMember;
            } else {
                return TopologyCommandKind::ReleaseContainer;
            }
        },
        command);
}

} // namespace QindaQt::Hybrid
