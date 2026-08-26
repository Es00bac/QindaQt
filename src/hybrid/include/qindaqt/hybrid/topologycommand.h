// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "layoutnode.h"

#include <QString>
#include <QtTypes>

#include <variant>

namespace QindaQt::Hybrid {

struct AddIndependentWindow final
{
    QString windowId;
};

// Removes a closed/unmanaged window from the session topology. Unlike detach,
// this command never reclassifies the removed ID as an independent window.
struct ForgetWindow final
{
    QString windowId;
};

struct DockIndependentWindows final
{
    QString containerId;
    QString pageId;
    QString firstWindowId;
    QString firstLeafNodeId;
    QString secondWindowId;
    QString secondLeafNodeId;
    QString splitNodeId;
    Core::SplitOrientation orientation = Core::SplitOrientation::Horizontal;
    double ratio = 0.5;
    Core::InsertPosition secondPosition = Core::InsertPosition::Second;
};

// Merging preserves both containers' page order and inserts the complete source
// page sequence at destinationPageIndex. The target's active page stays active.
struct MergeContainers final
{
    QString targetContainerId;
    QString sourceContainerId;
    qsizetype destinationPageIndex = 0;
};

// Moves one complete tab/page between containers while preserving its entire
// layout tree and all persisted structural IDs. This is intentionally distinct
// from MergeContainers and MoveMember: a dragged tab owns exactly one page.
struct MovePage final
{
    QString sourceContainerId;
    QString targetContainerId;
    QString pageId;
    qsizetype destinationPageIndex = 0;
};

// Detaches one tab/page from its container. A single-window page becomes an
// independent window; a split page keeps its complete tree in newContainerId.
struct DetachPage final
{
    QString sourceContainerId;
    QString pageId;
    QString newContainerId;
};

// Extracts exactly one member into a new page of its existing container. The
// original leaf ID is retained and the new page is inserted after targetPageId.
struct MoveMemberToPage final
{
    QString containerId;
    QString windowId;
    QString newPageId;
    QString targetPageId;
};

// Forms a new tabbed container from one complete source page and an
// independent target. The source page/tree IDs survive unchanged.
struct RegroupPageWithIndependent final
{
    QString sourceContainerId;
    QString pageId;
    QString independentWindowId;
    QString newContainerId;
    QString independentPageId;
    QString independentLeafNodeId;
};

struct MoveAsPage final
{
    QString pageId;
    qsizetype destinationPageIndex = 0;
};

struct MoveAsSplit final
{
    QString targetWindowId;
    QString splitNodeId;
    Core::SplitOrientation orientation = Core::SplitOrientation::Horizontal;
    double ratio = 0.5;
    Core::InsertPosition position = Core::InsertPosition::Second;
};

using MemberDestination = std::variant<MoveAsPage, MoveAsSplit>;

struct InsertIndependentWindow final
{
    QString targetContainerId;
    QString windowId;
    QString leafNodeId;
    MemberDestination destination;
};

struct GroupIndependentWindowsAsPages final
{
    QString containerId;
    QString firstWindowId;
    QString firstPageId;
    QString firstLeafNodeId;
    QString secondWindowId;
    QString secondPageId;
    QString secondLeafNodeId;
};

struct RegroupAsSplit final
{
    QString pageId;
    QString independentLeafNodeId;
    QString splitNodeId;
    Core::SplitOrientation orientation = Core::SplitOrientation::Horizontal;
    double ratio = 0.5;
    Core::InsertPosition memberPosition = Core::InsertPosition::Second;
};

struct RegroupAsPages final
{
    QString independentPageId;
    QString memberPageId;
    QString independentLeafNodeId;
};

using RegroupLayout = std::variant<RegroupAsSplit, RegroupAsPages>;

struct RegroupMemberWithIndependent final
{
    QString sourceContainerId;
    QString memberWindowId;
    QString independentWindowId;
    QString newContainerId;
    RegroupLayout layout;
};

struct MoveMember final
{
    QString sourceContainerId;
    QString targetContainerId;
    QString windowId;
    MemberDestination destination;
};

struct ReorderPage final
{
    QString containerId;
    QString pageId;
    qsizetype destinationPageIndex = 0;
};

struct ActivatePage final
{
    QString containerId;
    QString pageId;
};

struct ResizeSplit final
{
    QString containerId;
    QString splitNodeId;
    double ratio = 0.5;
};

// Member order is the leaf order in the existing tree. Reordering swaps the
// member payloads while preserving structural node IDs and split geometry.
struct ReorderMembers final
{
    QString containerId;
    QString firstWindowId;
    QString secondWindowId;
};

struct ReparentMember final
{
    QString containerId;
    QString windowId;
    QString targetWindowId;
    QString splitNodeId;
    Core::SplitOrientation orientation = Core::SplitOrientation::Horizontal;
    double ratio = 0.5;
    Core::InsertPosition position = Core::InsertPosition::Second;
};

struct DetachMember final
{
    QString containerId;
    QString windowId;
};

struct ReleaseContainer final
{
    QString containerId;
};

using TopologyCommand = std::variant<AddIndependentWindow,
                                     ForgetWindow,
                                     DockIndependentWindows,
                                     MergeContainers,
                                     MovePage,
                                     DetachPage,
                                     MoveMemberToPage,
                                     RegroupPageWithIndependent,
                                     InsertIndependentWindow,
                                     GroupIndependentWindowsAsPages,
                                     RegroupMemberWithIndependent,
                                     MoveMember,
                                     ReorderPage,
                                     ActivatePage,
                                     ResizeSplit,
                                     ReorderMembers,
                                     ReparentMember,
                                     DetachMember,
                                     ReleaseContainer>;

enum class TopologyCommandKind {
    AddIndependentWindow,
    ForgetWindow,
    DockIndependentWindows,
    MergeContainers,
    MovePage,
    DetachPage,
    MoveMemberToPage,
    RegroupPageWithIndependent,
    InsertIndependentWindow,
    GroupIndependentWindowsAsPages,
    RegroupMemberWithIndependent,
    MoveMember,
    ReorderPage,
    ActivatePage,
    ResizeSplit,
    ReorderMembers,
    ReparentMember,
    DetachMember,
    ReleaseContainer,
};

[[nodiscard]] TopologyCommandKind commandKind(const TopologyCommand &command) noexcept;

} // namespace QindaQt::Hybrid
